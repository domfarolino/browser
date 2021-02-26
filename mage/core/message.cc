#include "mage/core/message.h"

#include <sys/socket.h>

#include <memory>
#include <vector>

#include "mage/core/endpoint.h"

namespace mage {

SendInvitationMessage::SendInvitationMessage(Endpoint* endpoint_to_send) : Message(MessageType::SEND_INVITATION), endpoint_to_send_(endpoint_to_send) {}

std::vector<char> SendInvitationMessage::Serialize() {
  int payload_length = sizeof(MessageType) + (sizeof(char) * endpoint_to_send_->name.size());
  std::vector<char> buffer(payload_length);
  char* internal_buffer = buffer.data();

  // Serialize |type_|.
  // TODO(domfarolino): Use this.
  // int offset = 0;
  memcpy(internal_buffer, (char*)&type_, sizeof(MessageType));

  // Serialize |endpoint_to_send_|'s name.
  memcpy(&internal_buffer[4], endpoint_to_send_->name.data(), endpoint_to_send_->name.size());

  buffer.push_back('\0');
  return buffer;
}

// static
std::unique_ptr<Message> SendInvitationMessage::Deserialize(int fd) {
  size_t message_size = 16;
  char buffer[message_size];
  struct iovec iov = {buffer, message_size};
  char cmsg_buffer[CMSG_SPACE(message_size)];
  struct msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buffer;
  msg.msg_controllen = sizeof(cmsg_buffer);

  recvmsg(fd, &msg, /*non blocking*/MSG_DONTWAIT);
  printf("Port name: %s\n", buffer);

  return std::unique_ptr<SendInvitationMessage>(new SendInvitationMessage(nullptr));
}

}; // namspace mage
