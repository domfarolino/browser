#ifndef MAGE_CORE_CORE_H_
#define MAGE_CORE_CORE_H_

#include <map>
#include <memory>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/endpoint.h"
#include "mage/core/handles.h"
#include "mage/core/message.h"
#include "mage/core/node.h"

namespace mage {

// A global singleton for processes that initializes mage.
class Core {
 public:
  ~Core() = default;

  static void Init();
  // Always returns the global |Core| object for the current process.
  static Core* Get();

  static MageHandle SendInvitationAndGetMessagePipe(int fd) {
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
  static void BindReceiverDelegateToEndpoint(MageHandle local_handle, Endpoint::ReceiverDelegate* delegate) {
    auto endpoint_it = Get()->handle_table_.find(local_handle);
    CHECK_NE(endpoint_it, Get()->handle_table_.end());
    std::shared_ptr<Endpoint> endpoint = endpoint_it->second;
    endpoint->RegisterDelegate(delegate);
  }

  MageHandle GetNextMageHandle();
  void OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint);
  void RegisterLocalHandle(MageHandle local_handle, std::shared_ptr<Endpoint> local_endpoint);

 private:
  friend class MageTestWrapper;

  Core(): node_(new Node()) {}

  // A map of endpoints registered with this process, by MageHandle.
  std::map<MageHandle, std::shared_ptr<Endpoint>> handle_table_;

  // A map of all known endpoint channels, by node name.
  std::map<std::string, std::unique_ptr<Channel>> node_channel_map_;

  MageHandle next_available_handle_ = 1;

  std::function<void(MageHandle)> finished_accepting_invitation_callback_;

  std::unique_ptr<Node> node_;
};

}; // namespace mage

#endif // MAGE_CORE_CORE_H_
