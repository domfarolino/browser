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
  Message invitation_message(MessageType::SEND_INVITATION);
  MessageFragment<SendInvitationParams> params(invitation_message);
  params.Allocate();

  // Serialize inviter name.
  {
    MessageFragment<ArrayHeader<char>> array(invitation_message);
    array.AllocateArray(inviter_name.size());
    memcpy(array.data()->array_storage(), inviter_name.c_str(), inviter_name.size());
    params.data()->inviter_name.Set(array.data());
  }

  // Serialize temporary node name.
  {
    MessageFragment<ArrayHeader<char>> array(invitation_message);
    array.AllocateArray(remote_node_name_.size());
    memcpy(array.data()->array_storage(), remote_node_name_.c_str(), remote_node_name_.size());
    params.data()->temporary_remote_node_name.Set(array.data());
  }

  // Serialize intended endpoint name.
  {
    MessageFragment<ArrayHeader<char>> array(invitation_message);
    array.AllocateArray(intended_endpoint_name.size());
    memcpy(array.data()->array_storage(), intended_endpoint_name.c_str(), intended_endpoint_name.size());
    params.data()->intended_endpoint_name.Set(array.data());
  }

  // Serialize intended endpoint name.
  {
    MessageFragment<ArrayHeader<char>> array(invitation_message);
    array.AllocateArray(intended_endpoint_peer_name.size());
    memcpy(array.data()->array_storage(), intended_endpoint_peer_name.c_str(), intended_endpoint_peer_name.size());
    params.data()->intended_endpoint_peer_name.Set(array.data());
  }

  invitation_message.FinalizeSize();

  std::vector<char>& payload_buffer = invitation_message.payload_buffer();
  for (char c : payload_buffer) {
    printf("%02x ", c);
  }
  printf("\n");
  write(fd_, payload_buffer.data(), payload_buffer.size());
}

void Channel::SendAcceptInvitation(std::string temporary_remote_node_name,
                                   std::string actual_node_name) {
  Message message(MessageType::ACCEPT_INVITATION);
  MessageFragment<SendAcceptInvitationParams> params(message);
  params.Allocate();

  // Serialize temporary node name.
  {
    MessageFragment<ArrayHeader<char>> array(message);
    array.AllocateArray(temporary_remote_node_name.size());
    memcpy(array.data()->array_storage(), temporary_remote_node_name.c_str(), temporary_remote_node_name.size());
    params.data()->temporary_remote_node_name.Set(array.data());
  }

  // Serialize actual node name.
  {
    MessageFragment<ArrayHeader<char>> array(message);
    array.AllocateArray(actual_node_name.size());
    memcpy(array.data()->array_storage(), actual_node_name.c_str(), actual_node_name.size());
    params.data()->actual_node_name.Set(array.data());
  }

  message.FinalizeSize();

  std::vector<char>& payload_buffer = message.payload_buffer();
  for (char c : payload_buffer) {
    printf("%02x ", c);
  }
  printf("\n");

  write(fd_, payload_buffer.data(), payload_buffer.size());
}

void Channel::SendMessage(Message message) {
  std::vector<char>& payload_buffer = message.payload_buffer();
  /*
  printf("message size:: %d\n", payload_buffer.size());
  for (char c : payload_buffer) {
    printf("%02x ", c);
  }
  printf("\n");
  */

  write(fd_, payload_buffer.data(), payload_buffer.size());
}

void Channel::OnCanReadFromSocket() {
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

  // Pull the message header.
  MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
  Message message(header->type);

  {
    size_t message_size = header->size - sizeof(MessageHeader);
    char buffer[message_size];
    struct iovec iov = {buffer, message_size};
    char cmsg_buffer[CMSG_SPACE(message_size)];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buffer;
    msg.msg_controllen = sizeof(cmsg_buffer);
    recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    std::vector<char> byte_buffer(buffer, buffer + message_size);
    message.TakeBuffer(byte_buffer);
  }

  std::vector<char>& payload_buffer = message.payload_buffer();
  for (char c : payload_buffer) {
    printf("%02x ", c);
  }
  printf("\n");

  CHECK(delegate_);
  delegate_->OnReceivedMessage(message);
}

}; // namespace mage
