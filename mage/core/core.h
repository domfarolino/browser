#ifndef MAGE_CORE_CORE_H_
#define MAGE_CORE_CORE_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/scheduling/scheduling_handles.h"
#include "mage/core/endpoint.h"
#include "mage/core/handles.h"
#include "mage/core/message.h"

namespace mage {

class Channel;
class Node;

// A global singleton for processes that initializes mage.
class Core {
 public:
  // Main public APIs:

  // Relies on the IO thread's TaskLoop being synchronously accessible from the
  // UI thread.
  static void Init();
  static void ShutdownCleanly();

  // TODO(domfarolino): This should not actually be used outside of mage/core.
  static Core* Get();
  static std::vector<MageHandle> CreateMessagePipes();
  static MageHandle SendInvitationAndGetMessagePipe(
      int fd,
      base::OnceClosure callback = base::OnceClosure());
  static void AcceptInvitation(
      int fd,
      std::function<void(MageHandle)> finished_accepting_invitation_callback);
  static void SendMessage(MageHandle local_handle, Message message);
  static void BindReceiverDelegateToEndpoint(
      MageHandle local_handle,
      std::weak_ptr<Endpoint::ReceiverDelegate> delegate,
      std::shared_ptr<base::TaskRunner> delegate_task_runner);

  // More obscure helpers.

  // `handle_to_send` is about to be sent over an existing connection described
  // by `handle_of_preexisting_connection`. This method fills out
  // `endpoint_descriptor_to_populate`, which includes generating a new
  // `EndpointDescriptor::cross_node_endpoint_name` just in case
  // `handle_to_send` goes cross-node/process. If it does, `Node::SendMessage()`
  // will put the backing endpoint into the proxying state re-route this message
  // accordingly, and flush any queued messages from the backing endoint.
  // This happens if `handle_of_preexisting_endpoint` has a remote peer, or a
  // local peer that is proxying.
  static void PopulateEndpointDescriptor(
      MageHandle handle_to_send,
      MageHandle handle_of_preexisting_connection,
      EndpointDescriptor& endpoint_descriptor_to_populate);
  static MageHandle RecoverExistingMageHandleFromEndpointDescriptor(
      const EndpointDescriptor& endpoint_descriptor);
  static MageHandle RecoverNewMageHandleFromEndpointDescriptor(
      const EndpointDescriptor& endpoint_descriptor);

  MageHandle GetNextMageHandle();
  void OnReceivedAcceptInvitation();
  void OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint);
  void RegisterLocalHandleAndEndpoint(MageHandle local_handle,
                                      std::shared_ptr<Endpoint> local_endpoint);

 private:
  friend class MageTest;

  Core();

  // This is a `base::TaskRunner` pointing at the `base::TaskLoop` bound to the
  // thread that `this` is initialized on. Some `Core` methods are called on the
  // IO thread even though `this` may be set up from a different thread. If any
  // of our methods that run on the IO thread need to invoke a callback passed
  // in by the Mage initiator, we must invoke the callback that callback on the
  // thread the thread the initiator is running on, not just blindly the IO
  // thread. That's what we use this handle for.
  std::shared_ptr<base::TaskRunner> origin_task_runner_;

  // A map of endpoints registered with this process, keyed by MageHandle. This
  // can be accessed on multiple threads accessed on multiple threads, so access
  // must be guarded by `handle_table_lock_`. For example, if we don't take the
  // lock before accessing, one thread might try and `find()` an existing entry
  // while another thread is writing a new entry, which can break the first
  // thread's `find()`.
  std::map<MageHandle, std::shared_ptr<Endpoint>> handle_table_;
  // Used only by `this` for exclusive access to `handle_table_`.
  base::Mutex handle_table_lock_;

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

};  // namespace mage

#endif  // MAGE_CORE_CORE_H_
