#include "mage/core/channel.h"

#include <stdio.h>
#include <sys/socket.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/core.h"
#include "mage/core/endpoint.h"

namespace mage {

Channel::Channel(int fd, Delegate* delegate) : SocketReader(fd), delegate_(delegate) {}

void Channel::Start() {
  auto io_task_loop =
    std::static_pointer_cast<base::TaskLoopForIO>(base::GetIOThreadTaskLoop());
  io_task_loop->WatchSocket(this);
}

void Channel::SetRemoteNodeName(const std::string& name) {
  remote_node_name_ = name;
}

void Channel::SendInvitation(std::string inviter_name, std::string intended_endpoint_name, std::string intended_endpoint_peer_name) {
  Message message(MessageType::SEND_INVITATION);
  MessageFragment<SendInvitationParams> params(message);
  params.Allocate();

  // Serialize inviter name.
  memcpy(params.data()->inviter_name, inviter_name.c_str(),
         inviter_name.size());

  // Serialize temporary remote node name.
  memcpy(params.data()->temporary_remote_node_name, remote_node_name_.c_str(),
         remote_node_name_.size());

  // Serialize intended endpoint name.
  memcpy(params.data()->intended_endpoint_name, intended_endpoint_name.c_str(),
         intended_endpoint_name.size());

  // Serialize intended endpoint peer name.
  memcpy(params.data()->intended_endpoint_peer_name,
         intended_endpoint_peer_name.c_str(),
         intended_endpoint_peer_name.size());

  message.FinalizeSize();
  SendMessage(std::move(message));
}

void Channel::SendAcceptInvitation(std::string temporary_remote_node_name,
                                   std::string actual_node_name) {
  Message message(MessageType::ACCEPT_INVITATION);
  MessageFragment<SendAcceptInvitationParams> params(message);
  params.Allocate();

  // Serialize temporary node name.
  memcpy(params.data()->temporary_remote_node_name,
         temporary_remote_node_name.c_str(), temporary_remote_node_name.size());

  // Serialize actual node name.
  memcpy(params.data()->actual_node_name, actual_node_name.c_str(),
         actual_node_name.size());

  message.FinalizeSize();
  SendMessage(std::move(message));
}

void Channel::SendMessage(Message message) {
  std::vector<char>& payload_buffer = message.payload_buffer();
  for (char c : payload_buffer) {
    printf("%02x ", c);
  }
  printf("\n");

  write(fd_, payload_buffer.data(), payload_buffer.size());
}

void Channel::OnCanReadFromSocket() {
  std::vector<char> full_message_buffer;

  // Read the message header.
  {
    size_t header_size = sizeof(MessageHeader);
    char buffer[header_size];
    struct iovec iov = {buffer, header_size};
    char cmsg_buffer[CMSG_SPACE(header_size)];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buffer;
    msg.msg_controllen = sizeof(cmsg_buffer);

    recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    std::vector<char> tmp_header_buffer(buffer, buffer + header_size);
    full_message_buffer = std::move(tmp_header_buffer);
  }

  // Pull out the message header.
  MessageHeader* header = reinterpret_cast<MessageHeader*>(full_message_buffer.data());
  full_message_buffer.resize(header->size);

  // Read the message body.
  {
    size_t body_size = header->size - sizeof(MessageHeader);
    char buffer[body_size];
    struct iovec iov = {buffer, body_size};
    char cmsg_buffer[CMSG_SPACE(body_size)];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buffer;
    msg.msg_controllen = sizeof(cmsg_buffer);

    recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    memcpy(full_message_buffer.data() + sizeof(MessageHeader), buffer, body_size);
  }

  Message message(header->type);
  message.ConsumeBuffer(std::move(full_message_buffer));

  std::vector<char>& payload_buffer = message.payload_buffer();
  for (char c : payload_buffer) {
    printf("%02x ", c);
  }
  printf("\n");

  CHECK(delegate_);
  delegate_->OnReceivedMessage(std::move(message));
}

}; // namespace mage
