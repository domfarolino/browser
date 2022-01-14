#ifndef MAGE_CORE_CORE_H_
#define MAGE_CORE_CORE_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/endpoint.h"
#include "mage/core/handles.h"
#include "mage/core/message.h"
#include "mage/core/node.h"

namespace mage {

// A global singleton for processes that initializes mage.
class Core {
 public:
  static void Init();
  static void ShutdownCleanly();

  // Always returns the global |Core| object for the current process.
  static Core* Get();

  static std::vector<MageHandle> CreateMessagePipes() {
    std::vector<MageHandle> return_handles = Get()->node_->CreateMessagePipes();
    CHECK_NE(Get()->handle_table_.find(return_handles[0]), Get()->handle_table_.end());
    CHECK_NE(Get()->handle_table_.find(return_handles[1]), Get()->handle_table_.end());
    return return_handles;
  }
  static MageHandle SendInvitationAndGetMessagePipe(
      int fd, base::OnceClosure callback = base::OnceClosure()) {
    Get()->remote_has_accepted_invitation_callback_ = std::move(callback);
    return Get()->node_->SendInvitationAndGetMessagePipe(fd);
  }
  static void AcceptInvitation(int fd, std::function<void(MageHandle)> finished_accepting_invitation_callback) {
    Get()->finished_accepting_invitation_callback_ = std::move(finished_accepting_invitation_callback);
    Get()->node_->AcceptInvitation(fd);
  }
  static void SendMessage(MageHandle local_handle, Message message) {
    auto endpoint_it = Get()->handle_table_.find(local_handle);
    CHECK_NE(endpoint_it, Get()->handle_table_.end());
    printf("Core::SendMessage\n");
    Get()->node_->SendMessage(endpoint_it->second, std::move(message));
  }
  static void BindReceiverDelegateToEndpoint(
      MageHandle local_handle,
      Endpoint::ReceiverDelegate* delegate,
      std::shared_ptr<base::TaskRunner> delegate_task_runner) {
    auto endpoint_it = Get()->handle_table_.find(local_handle);
    CHECK_NE(endpoint_it, Get()->handle_table_.end());
    std::shared_ptr<Endpoint> endpoint = endpoint_it->second;
    endpoint->RegisterDelegate(delegate, std::move(delegate_task_runner));
  }
  // This method takes a handle that is about to be sent over an existing
  // connection, and a handle that represents said existing connection. If the
  // handle representing the existing connection indeed has a remote peer, that
  // means the handle-to-send is going to be sent cross-process. In this case,
  // we must find the endpoint associated with it, and put it in a proxying
  // state so that it knows how to forward things to the non-proxying endpoint
  // in the remote node.
  static void PutHandleToSendInProxyingStateIfTargetIsRemote(
      MageHandle handle_to_send,
      MageHandle local_handle,
      EndpointInfo& endpoint_info_to_populate) {
    std::shared_ptr<Endpoint> local_endpoint = Get()->handle_table_.find(local_handle)->second;
    CHECK(local_endpoint);

    std::string peer_node_name = local_endpoint->peer_address.node_name;
    std::string peer_endpoint_name = local_endpoint->peer_address.endpoint_name;
    // We do nothing if `handle_to_send` will be sent locally.
    if (peer_node_name == Get()->node_->name_) {
      printf("*****************PutHandleToSendInProxyingStateIfTargetIsRemote() early return\n");
      return;
    }
    printf("**************PutHandleToSendInProxyingStateIfTargetIsRemote() CONTINUGIN\n");

    // At this point we know that `handle_to_send` is going to be sent to a
    // remote peer node. This means we have to put the `Endpoint` it represents
    // into a proxying state, so whenever it receives messages, it forwards them
    // to the new endpoint that the remote peer node will set up for it.
    std::shared_ptr<Endpoint> endpoint_to_proxy = Get()->handle_table_.find(handle_to_send)->second;
    CHECK_EQ(Get()->node_->name_, endpoint_to_proxy->peer_address.node_name);
    memcpy(endpoint_info_to_populate.endpoint_name, endpoint_to_proxy->name.c_str(), kIdentifierSize);
    memcpy(endpoint_info_to_populate.peer_node_name, endpoint_to_proxy->peer_address.node_name.c_str(), kIdentifierSize);
    memcpy(endpoint_info_to_populate.peer_endpoint_name, endpoint_to_proxy->peer_address.endpoint_name.c_str(), kIdentifierSize);
    printf("    PutHandl() peer_node_name: %s\n", peer_node_name.c_str());
    endpoint_to_proxy->SetProxying(/*node_to_proxy_to=*/peer_node_name);
  }
  static MageHandle RecoverMageHandleFromEndpointInfo(EndpointInfo& endpoint_info) {
    printf("Core::RecoverMageHandleFromEndpointInfo(ep)\n");
    printf("endpoint_info: %s\n", endpoint_info.endpoint_name);
    printf("endpoint_info: %s\n", endpoint_info.peer_node_name);
    printf("endpoint_info: %s\n", endpoint_info.peer_endpoint_name);

    std::shared_ptr<Endpoint> local_endpoint(new Endpoint());
    std::string endpoint_name(endpoint_info.endpoint_name,
                              endpoint_info.endpoint_name + kIdentifierSize);

    local_endpoint->name = endpoint_name;
    MageHandle local_handle = Core::Get()->GetNextMageHandle();
    Core::Get()->RegisterLocalHandle(local_handle, local_endpoint);
    return local_handle;
  }

  MageHandle GetNextMageHandle();
  void OnReceivedAcceptInvitation();
  void OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint);
  void RegisterLocalHandle(MageHandle local_handle, std::shared_ptr<Endpoint> local_endpoint);

 private:
  friend class MageTest;

  Core(): origin_task_runner_(base::GetCurrentThreadTaskRunner()),
          node_(new Node()) {}

  // This is a `base::TaskRunner` pointing at the `base::TaskLoop` bound to the
  // thread that `this` is initialized on. Some `Core` methods are called on the
  // IO thread even though `this` may be set up from a different thread. If any
  // of our methods that run on the IO thread need to invoke a callback passed
  // in by the Mage initiator, we must invoke the callback that callback on the
  // thread the thread the initiator is running on, not just blindly the IO
  // thread. That's what we use this handle for.
  std::shared_ptr<base::TaskRunner> origin_task_runner_;

  // A map of endpoints registered with this process, by MageHandle.
  std::map<MageHandle, std::shared_ptr<Endpoint>> handle_table_;

  // A map of all known endpoint channels, by node name.
  std::map<std::string, std::unique_ptr<Channel>> node_channel_map_;

  MageHandle next_available_handle_ = 1;

  // This is optionally supplied when sending an invitation. It reports back
  // when the remote process has accepted the invitation. Guaranteed to be
  // called asynchronously. Mostly used for tests.
  base::OnceClosure remote_has_accepted_invitation_callback_;
  // This is mandatorily supplied by the invitee when attempting to accept an
  // invitation. Accepting an invitation is asynchronous since we have to wait
  // for the invitation to arrive. Guaranteed to be called asynchronously.
  std::function<void(MageHandle)> finished_accepting_invitation_callback_;

  std::unique_ptr<Node> node_;
};

}; // namespace mage

#endif // MAGE_CORE_CORE_H_
