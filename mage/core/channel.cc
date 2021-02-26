#include "mage/core/channel.h"

#include <sys/socket.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "mage/core/core.h"
#include "mage/core/endpoint.h"

namespace mage {

Channel::Channel(int fd) : SocketReader(fd) {}

void Channel::Start() {
  Core::GetTaskLoop()->WatchSocket(fd_, this);
}

void Channel::SetRemoteNodeName(const std::string& name) {
  remote_node_name_ = name;
}

void Channel::SendInvitation(Endpoint* remote_endpoint) {
  SendInvitationMessage invite(remote_endpoint);
  std::vector<char> buffer = invite.Serialize();
  printf("buffer size: %lu\n", buffer.size());
  write(fd_, buffer.data(), buffer.size());
}

void Channel::OnCanReadFromSocket() {
  printf("Reading from the socket!\n");

  MessageType message_type;
  size_t message_size = sizeof(MessageType);
  char buffer[message_size];
  struct iovec iov = {buffer, message_size};
  char cmsg_buffer[CMSG_SPACE(message_size)];
  struct msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buffer;
  msg.msg_controllen = sizeof(cmsg_buffer);

  // Deserialize MessageType from the message;
  recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
  message_type = *(MessageType*)&buffer;

  printf("MessageType: %d\n", message_type);

  std::unique_ptr<Message> deserialized_message;
  switch (message_type) {
    case MessageType::SEND_INVITATION:
      deserialized_message = SendInvitationMessage::Deserialize(fd_);
      break;
    case MessageType::ACCEPT_INVITATION:
      NOTREACHED();
      break;
    case MessageType::USER_MESSAGE:
      NOTREACHED();
      break;
  }
}

}; // namespace mage
