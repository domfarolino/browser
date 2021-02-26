#include "mage/core/message.h"

#include <sys/socket.h>

#include <memory>
#include <vector>

#include "mage/core/endpoint.h"

namespace mage {

std::vector<char> SendInvitationMessage::Serialize() {
  int payload_length = sizeof(MessageType) +
                      (sizeof(char) * inviter_name_.size()) +
                      (sizeof(char) * temporary_remote_node_name_.size()) +
                      (sizeof(char) * intended_peer_endpoint_name_.size());
  std::vector<char> buffer(payload_length);
  char* internal_buffer = buffer.data();

  // Serialize |type_|.
  int offset = 0;
  memcpy(&internal_buffer[offset], (char*)&type_, sizeof(MessageType));
  offset += sizeof(MessageType);

  // Serialize |inviter_name_|.
  memcpy(&internal_buffer[offset], inviter_name_.data(), inviter_name_.size());
  offset += inviter_name_.size();

  // Serialize |temporary_remote_node_name_|.
  memcpy(&internal_buffer[offset], temporary_remote_node_name_.data(), temporary_remote_node_name_.size());
  offset += temporary_remote_node_name_.size();

  // Serialize |intended_peer_endpoint_name_|.
  memcpy(&internal_buffer[offset], intended_peer_endpoint_name_.data(), intended_peer_endpoint_name_.size());
  offset += intended_peer_endpoint_name_.size();

  buffer.push_back('\0');
  return buffer;
}

// static
std::unique_ptr<Message> SendInvitationMessage::Deserialize(int fd) {
  size_t message_size = 50 - sizeof(MessageType);
  char buffer[message_size];
  struct iovec iov = {buffer, message_size};
  char cmsg_buffer[CMSG_SPACE(message_size)];
  struct msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buffer;
  msg.msg_controllen = sizeof(cmsg_buffer);

  recvmsg(fd, &msg, /*non blocking*/MSG_DONTWAIT);

  int offset = 0;

  // Deserialize to |inviter_name|.
  std::string inviter_name(&buffer[offset], &buffer[offset] + 15);
  offset += inviter_name.size();

  // Deserialize to |temporary_remote_node_name|.
  std::string temporary_remote_node_name(&buffer[offset], &buffer[offset] + 15);
  offset += temporary_remote_node_name.size();

  // Deserialize |intended_peer_endpoint_name|.
  std::string intended_peer_endpoint_name(&buffer[offset], &buffer[offset] + 15);
  offset += intended_peer_endpoint_name.size();

  printf("ACCEPT_INVITATION deserialization done\n");
  printf("  inviter_name:                %s\n", inviter_name.c_str());
  printf("  temporary_remote_node_name:  %s\n", temporary_remote_node_name.c_str());
  printf("  intended_peer_endpoint_name: %s\n", intended_peer_endpoint_name.c_str());
  return std::unique_ptr<SendInvitationMessage>(new SendInvitationMessage());
}

}; // namspace mage
