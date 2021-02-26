#include "mage/core/message.h"

#include <sys/socket.h>

#include <memory>
#include <vector>

#include "mage/core/endpoint.h"

namespace mage {

std::vector<char> SendInvitationMessage::Serialize() {
  int payload_length = sizeof(MessageType) +
                      (sizeof(char) * inviter_name.size()) +
                      (sizeof(char) * temporary_remote_node_name.size()) +
                      (sizeof(char) * intended_endpoint_name.size()) +
                      (sizeof(char) * intended_endpoint_peer_name.size());
  std::vector<char> buffer(payload_length);
  char* internal_buffer = buffer.data();

  // Serialize |type|.
  int offset = 0;
  memcpy(&internal_buffer[offset], (char*)&type, sizeof(MessageType));
  offset += sizeof(MessageType);

  // Serialize |inviter_name|.
  memcpy(&internal_buffer[offset], inviter_name.data(), inviter_name.size());
  offset += inviter_name.size();

  // Serialize |temporary_remote_node_name|.
  memcpy(&internal_buffer[offset], temporary_remote_node_name.data(), temporary_remote_node_name.size());
  offset += temporary_remote_node_name.size();

  // Serialize |intended_endpoint_name|.
  memcpy(&internal_buffer[offset], intended_endpoint_name.data(), intended_endpoint_name.size());
  offset += intended_endpoint_name.size();

  // Serialize |intended_endpoint_peer_name|.
  memcpy(&internal_buffer[offset], intended_endpoint_peer_name.data(), intended_endpoint_peer_name.size());
  offset += intended_endpoint_peer_name.size();

  buffer.push_back('\0');
  return buffer;
}

// static
std::unique_ptr<Message> SendInvitationMessage::Deserialize(int fd) {
  size_t message_size = 65 - sizeof(MessageType);
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

  // Deserialize |intended_endpoint_name|.
  std::string intended_endpoint_name(&buffer[offset], &buffer[offset] + 15);
  offset += intended_endpoint_name.size();

  // Deserialize |intended_endpoint_peer_name|.
  std::string intended_endpoint_peer_name(&buffer[offset], &buffer[offset] + 15);
  offset += intended_endpoint_peer_name.size();

  printf("ACCEPT_INVITATION deserialization done\n");
  printf("  inviter_name:                %s\n", inviter_name.c_str());
  printf("  temporary_remote_node_name:  %s\n", temporary_remote_node_name.c_str());
  printf("  intended_endpoint_name: %s\n", intended_endpoint_name.c_str());
  printf("  intended_endpoint_peer_name: %s\n", intended_endpoint_peer_name.c_str());

  std::unique_ptr<SendInvitationMessage> message(new SendInvitationMessage());
  message->type = MessageType::SEND_INVITATION;
  message->inviter_name = inviter_name;
  message->temporary_remote_node_name = temporary_remote_node_name;
  message->intended_endpoint_name = intended_endpoint_name;
  message->intended_endpoint_peer_name = intended_endpoint_peer_name;
  return message;
}

}; // namspace mage
