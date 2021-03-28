#ifndef MAGE_CORE_CORE_H_
#define MAGE_CORE_CORE_H_

#include <map>
#include <memory>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/endpoint.h"
#include "mage/core/handles.h"
#include "mage/core/node.h"

namespace mage {

// A global singleton for processes that initializes mage.
class Core {
 public:
  ~Core() = default;

  static void Init();
  // Always returns the global |Core| object for the current process.
  static Core* Get();

  static MageHandle SendInvitationToTargetNodeAndGetMessagePipe(int fd) {
    return Get()->node_->SendInvitationToTargetNodeAndGetMessagePipe(fd);
  }
  static void AcceptInvitation(int fd, std::function<void(MageHandle)> received_invitation_handler) {
    Get()->async_invitation_handler_ = std::move(received_invitation_handler);
    Get()->node_->AcceptInvitation(fd);
  }

  MageHandle GetNextMageHandle();
  void OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint);

 private:
  Core(): node_(new Node()) {}

  // A map of endpoints registered with this process, by Handle
  std::map<MageHandle, std::shared_ptr<Endpoint>> handle_table_;

  // A map of all known endpoint channels, by node name.
  std::map<std::string, std::unique_ptr<Channel>> node_channel_map_;

  MageHandle next_available_handle_ = 1;

  std::function<void(MageHandle)> async_invitation_handler_;

  std::unique_ptr<Node> node_;
};

}; // namespace mage

#endif // MAGE_CORE_CORE_H_
