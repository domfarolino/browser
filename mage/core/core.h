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

  // Main public APIs:
  // TODO(domfarolino): This should not actually be used outside of mage/core.
  static Core* Get();
  static std::vector<MageHandle> CreateMessagePipes();
  static MageHandle SendInvitationAndGetMessagePipe(int fd, base::OnceClosure callback = base::OnceClosure());
  static void AcceptInvitation(int fd, std::function<void(MageHandle)> finished_accepting_invitation_callback);
  static void SendMessage(MageHandle local_handle, Message message);
  static void BindReceiverDelegateToEndpoint(MageHandle local_handle, std::weak_ptr<Endpoint::ReceiverDelegate> delegate, std::shared_ptr<base::TaskRunner> delegate_task_runner);

  // More obscure helpers.

  // This method takes a handle `handle_to_send` that is about to be sent over
  // an existing connection described by
  // `local_handle_of_preexisting_connection`. If the handle representing the
  // existing connection indeed has a remote peer [TODO(domfarolino): What does
  // this mean???? Will it actually have a remote peer or a local peer that is
  // in the proxying state? I think it should be the latter] then
  // `handle_to_send` is being sent cross-process. In this case, we must find
  // the endpoint associated with it, and put it in a proxying state so that it
  // knows how to forward things to the non-proxying endpoint in the remote node.
  static void PopulateEndpointDescriptorAndMaybeSetEndpointInProxyingState(MageHandle handle_to_send, MageHandle local_handle_of_preexisting_connection, EndpointDescriptor& endpoint_descriptor_to_populate);
  static MageHandle RecoverExistingMageHandleFromEndpointDescriptor(const EndpointDescriptor& endpoint_descriptor);
  static MageHandle RecoverNewMageHandleFromEndpointDescriptor(const EndpointDescriptor& endpoint_descriptor);

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
