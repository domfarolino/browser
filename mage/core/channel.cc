#include "mage/core/channel.h"

#include <sys/socket.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "mage/core/core.h"
#include "mage/core/endpoint.h"

namespace mage {

Channel::Channel(int fd, Delegate* delegate) : SocketReader(fd), delegate_(delegate) {}

void Channel::Start() {
  Core::GetTaskLoop()->WatchSocket(fd_, this);
}

void Channel::SetRemoteNodeName(const std::string& name) {
  remote_node_name_ = name;
}

void Channel::SendInvitation(std::string inviter_name, std::string intended_endpoint_name, std::string intended_endpoint_peer_name) {
  SendInvitationMessage invitation;
  invitation.inviter_name = inviter_name;
  invitation.temporary_remote_node_name = remote_node_name_;
  invitation.intended_endpoint_name = intended_endpoint_name;
  invitation.intended_endpoint_peer_name = intended_endpoint_peer_name;

  std::vector<char> buffer = invitation.Serialize();
  printf("Channel::SendInvitation buffer size: %lu\n", buffer.size());
  write(fd_, buffer.data(), buffer.size());
}

void Channel::OnCanReadFromSocket() {
  size_t message_size = sizeof(MessageType);
  char buffer[message_size];
  struct iovec iov = {buffer, message_size};
  char cmsg_buffer[CMSG_SPACE(message_size)];
  struct msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buffer;
  msg.msg_controllen = sizeof(cmsg_buffer);

  // Deserialize MessageType from the message.
  MessageType message_type;
  recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
  message_type = *(MessageType*)&buffer;

  std::unique_ptr<Message> deserialized_message;
  switch (message_type) {
    case MessageType::SEND_INVITATION:
      printf("Received message type: SEND_INVITATION\n");
      deserialized_message = SendInvitationMessage::Deserialize(fd_);
      break;
    case MessageType::ACCEPT_INVITATION:
      NOTREACHED();
      break;
    case MessageType::USER_MESSAGE:
      NOTREACHED();
      break;
  }

  CHECK(delegate_);
  delegate_->OnReceivedMessage(std::move(deserialized_message));
}

}; // namespace mage
