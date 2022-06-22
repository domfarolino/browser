#ifndef MAGE_CORE_CORE_H_
#define MAGE_CORE_CORE_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/scheduling/scheduling_handles.h"
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
    printf("Core::SendMessage\n");
    CHECK_NE(endpoint_it, Get()->handle_table_.end());
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
  // This method takes a handle `handle_to_send` that is about to be sent over
  // an existing connection described by
  // `local_handle_of_preexisting_connection`. If the handle representing the
  // existing connection indeed has a remote peer [TODO(domfarolino): What does
  // this mean???? Will it actually have a remote peer or a local peer that is
  // in the proxying state? I think it should be the latter] then
  // `handle_to_send` is being sent cross-process. In this case, we must find
  // the endpoint associated with it, and put it in a proxying state so that it
  // knows how to forward things to the non-proxying endpoint in the remote node.
  static void PopulateEndpointDescriptorAndMaybeSetEndpointInProxyingState(
      MageHandle handle_to_send,
      MageHandle local_handle_of_preexisting_connection,
      EndpointDescriptor& endpoint_descriptor_to_populate) {
    std::shared_ptr<Endpoint> local_endpoint_of_preexisting_connection = Get()->handle_table_.find(local_handle_of_preexisting_connection)->second;
    CHECK(local_endpoint_of_preexisting_connection);

    std::string peer_node_name = local_endpoint_of_preexisting_connection->peer_address.node_name;
    std::string peer_endpoint_name = local_endpoint_of_preexisting_connection->peer_address.endpoint_name;

    printf("**************PutHandleToSendInProxyingStateIfTargetIsRemote() populating EndpointDescriptor:\n");
    printf("    sending endpoint name: %s\n", local_endpoint_of_preexisting_connection->name.c_str());
    printf("    sending endpoint [peer node: %s]\n", peer_node_name.c_str());
    printf("    sending endpoint [peer endpoint: %s]\n", peer_endpoint_name.c_str());

    // Populating an `EndpointDescriptor` that is bound for another process is
    // really easy.
    //   1.) An endpoint's name never changes regardless of what process it
    //       lives in, so we can get that information from the endpoint in this
    //       process before we "send it".
    //   2.) Its peer node name when it lives in another process is just the
    //       current process that `endpoint_being_sent` lives in before being
    //       "sent", so we already have that information.
    //   3.) Its peer endpoint's name that this endpoint will have once it lives
    //       in another process is also just its current peer endpoint's name,
    //       since again that will never change.
    std::shared_ptr<Endpoint> endpoint_being_sent = Get()->handle_table_.find(handle_to_send)->second;
    CHECK_EQ(Get()->node_->name_, endpoint_being_sent->peer_address.node_name);
    memcpy(endpoint_descriptor_to_populate.endpoint_name, endpoint_being_sent->name.c_str(), kIdentifierSize);
    memcpy(endpoint_descriptor_to_populate.peer_node_name, endpoint_being_sent->peer_address.node_name.c_str(), kIdentifierSize);
    memcpy(endpoint_descriptor_to_populate.peer_endpoint_name, endpoint_being_sent->peer_address.endpoint_name.c_str(), kIdentifierSize);
    printf("endpoint_descriptor_to_populate.endpoint_name: %s\n", endpoint_descriptor_to_populate.endpoint_name);
    printf("endpoint_descriptor_to_populate.peer_node_name: %s\n", endpoint_descriptor_to_populate.peer_node_name);
    printf("endpoint_descriptor_to_populate.peer_endpoint_name: %s\n", endpoint_descriptor_to_populate.peer_endpoint_name);

    // If `handle_to_send` is only being sent locally (staying in the same
    // process), we do nothing else; we don't put `endpoint_being_sent` in the
    // proxying state. We only put endpoints into the proxying state when they
    // are traveling to another node, and therefore have to proxy messages to
    // another node to find the final non-proxying endpoint.
    //
    // Note that it is possible `handle_to_send` is being sent over a purely
    // local connection whose other end has not been bound yet. If the other end
    // gets sent to a remote node before being bound, we'll obviously have to
    // forward all of the messages that were queued over the preexisting local
    // connection to the remote peer. But some of those messages could contain
    // handles that also have been queueing their own messages. We have to
    // recursively flush all queued messages across all sent handles/endpoints
    // to the remote node in this case, and set each of the corresponding local
    // peer endpoints into a proxying state. This happens in:
    // TODO(domfarolino): Figure out where this should finally happen.
    if (peer_node_name == Get()->node_->name_) {
      printf("*****************PutHandleToSendInProxyingStateIfTargetIsRemote() returning early without putting endpoint into proxy mode, since it is not going remote\n");
      return;
    }

    endpoint_being_sent->SetProxying(/*node_to_proxy_to=*/peer_node_name);
  }
  static MageHandle RecoverMageHandleFromEndpointDescriptor(EndpointDescriptor& endpoint_descriptor) {
    printf("Core::RecoverMageHandleFromEndpointDescriptor(endpoint_descriptor)\n");
    printf("  endpoint_descriptor: %s\n", endpoint_descriptor.endpoint_name);
    printf("  endpoint_descriptor: %s\n", endpoint_descriptor.peer_node_name);
    printf("  endpoint_descriptor: %s\n", endpoint_descriptor.peer_endpoint_name);

    std::shared_ptr<Endpoint> local_endpoint(new Endpoint());
    std::string endpoint_name(endpoint_descriptor.endpoint_name,
                              endpoint_descriptor.endpoint_name + kIdentifierSize);

    local_endpoint->name = endpoint_name;
    MageHandle local_handle = Core::Get()->GetNextMageHandle();
    Core::Get()->RegisterLocalHandle(local_handle, std::move(local_endpoint));
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
