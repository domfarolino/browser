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
  // TODO(domfarolino): This is temporary to let `Endpoint` forward messages
  // that it gets when it is in the proxying state. Really we should have a
  // cleaner way to access node from endpoint.
  static void ForwardMessage(std::shared_ptr<Endpoint> endpoint, Message message) {
    CHECK(endpoint->state == Endpoint::State::kUnboundAndProxying);

    if (endpoint->proxy_target.node_name == Get()->node_->name_) {
      // TODO(domfarolino): We need a test for this case.
      NOTREACHED();
    } else {
      Get()->node_->node_channel_map_[endpoint->proxy_target.node_name]->SendMessage(std::move(message));
    }
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

    printf("**************PopulateEndpointDescriptorAndMaybeSetEndpointInProxyingState() populating EndpointDescriptor:\n");
    printf("    sending endpoint name: %s\n", local_endpoint_of_preexisting_connection->name.c_str());
    printf("    sending endpoint [peer node: %s]\n", peer_node_name.c_str());
    printf("    sending endpoint [peer endpoint: %s]\n", peer_endpoint_name.c_str());

    // Populating an `EndpointDescriptor` is easy regardless of whether it is
    // being sent same-process or cross-process.
    //   1.) Fill out the name of the endpoint that we are sending. This is used
    //       in case the descriptor is sent to a same-process endpoint, in which
    //       case we don't actually create a "new" endpoint from this
    //       descriptor, but we know to just target the already-existing one
    //       with this name.
    //   2.) Generate and fill out a new `cross_node_endpoint_name`: this is
    //       used as the target endpoint's name upon endpoint creation if the
    //       descriptor is sent cross-process. We generate this up-front so that
    //       if the descriptor does go cross-process, the sending endpoint
    //       automatically knows the name by which to target the remote
    //       endpoint. This name is used in the `proxy_target` of the endpoint
    //       (in *this* process) that we're "sending".
    // TODO(domfarolino): I think there is a bug with the following two steps.
    // See the test case in https://github.com/domfarolino/browser/pull/32 that
    // starts with "Process A creates two endpoints.
    //   3.) The target endpoint's peer node name when it lives in another other
    //       process is just the current endpoint's peer node name.
    //   4.) Same as (3), for the peer's endpoint name.
    std::shared_ptr<Endpoint> endpoint_being_sent = Get()->handle_table_.find(handle_to_send)->second;
    memcpy(endpoint_descriptor_to_populate.endpoint_name, endpoint_being_sent->name.c_str(), kIdentifierSize);
    std::string cross_node_endpoint_name = util::RandomIdentifier();
    memcpy(endpoint_descriptor_to_populate.cross_node_endpoint_name, cross_node_endpoint_name.c_str(), kIdentifierSize);
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
    if (peer_node_name == Get()->node_->name_) {
      printf("*****************PopulateEndpointDescriptorAndMaybeSetEndpointInProxyingState() returning early without putting endpoint into proxy mode, since it is not going remote\n");
      return;
    }

    endpoint_being_sent->SetProxying(/*in_node_name=*/peer_node_name, /*in_endpoint_name=*/cross_node_endpoint_name);
  }

  static MageHandle RecoverExistingMageHandleFromEndpointDescriptor(
      const EndpointDescriptor& endpoint_descriptor) {
    printf("Core::RecoverExistingMageHandleFromEndpointDescriptor(endpoint_descriptor)\n");
    std::string endpoint_name(endpoint_descriptor.endpoint_name,
                              endpoint_descriptor.endpoint_name + kIdentifierSize);

    std::map<MageHandle, std::shared_ptr<Endpoint>>& handle_table =
        Core::Get()->handle_table_;
    // First see if the endpoint is already registered. If so, just early-return
    // the handle associated with it.
    for (std::map<MageHandle, std::shared_ptr<Endpoint>>::const_iterator it =
             handle_table.begin();
         it != handle_table.end(); it++) {
      if (it->second->name == endpoint_name) {
        return it->first;
      }
    }

    NOTREACHED();
  }

  static MageHandle RecoverNewMageHandleFromEndpointDescriptor(const EndpointDescriptor& endpoint_descriptor) {
    printf("Core::RecoverMageHandleFromEndpointDescriptor(endpoint_descriptor)\n");
    endpoint_descriptor.Print();

    std::string cross_node_endpoint_name(
        endpoint_descriptor.cross_node_endpoint_name,
        endpoint_descriptor.cross_node_endpoint_name + kIdentifierSize);

    std::shared_ptr<Endpoint> local_endpoint(new Endpoint());
    // When we recover a new endpoint from a remote endpoint, the name we should
    // create it with is `cross_node_endpoint_name`, because this is the name
    // that the originator process generated for us so that it knows how to
    // target the remote endpoint.
    local_endpoint->name = cross_node_endpoint_name;
    local_endpoint->peer_address.node_name.assign(endpoint_descriptor.peer_node_name, kIdentifierSize);
    local_endpoint->peer_address.endpoint_name.assign(endpoint_descriptor.peer_endpoint_name, kIdentifierSize);
    MageHandle local_handle = Core::Get()->GetNextMageHandle();
    Core::Get()->RegisterLocalHandleAndEndpoint(local_handle, std::move(local_endpoint));
    return local_handle;
  }

  MageHandle GetNextMageHandle();
  void OnReceivedAcceptInvitation();
  void OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint);
  void RegisterLocalHandleAndEndpoint(MageHandle local_handle, std::shared_ptr<Endpoint> local_endpoint);

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
