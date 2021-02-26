#ifndef MAGE_CORE_CHANNEL_H_
#define MAGE_CORE_CHANNEL_H_

#include <string>

#include "base/check.h"
#include "base/scheduling/task_loop_for_io.h"

namespace mage {

class Message;

class Channel : public base::TaskLoopForIO::SocketReader {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnReceivedMessage(std::unique_ptr<Message> message) = 0;
  };

  Channel(int fd, Delegate* delegate);
  virtual ~Channel() = default;

  void Start();
  void SetRemoteNodeName(const std::string& name);
  void SendInvitation(std::string inviter_name, std::string intended_endpoint_name, std::string intended_endpint_peer_nae);

  // base::TaskLoopForIO::SocketReader implementation:
  void OnCanReadFromSocket() override;

 private:
  // This is always initialized as something temporary until we hear back from
  // the remote node and it tells us its name.
  std::string remote_node_name_;

  // The |Delegate| owns |this|, so this pointer will never be null.
  Delegate* delegate_;
};

}; // namespace mage

#endif // MAGE_CORE_CHANNEL_H_
