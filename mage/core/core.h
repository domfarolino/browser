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
