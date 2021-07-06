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

namespace {

// Helper function to print the full human-readable contents of a mage::Message.
void PrintFullMessageContents(Message& message) {
  CHECK_ON_THREAD(base::ThreadType::IO);

  std::vector<char>& payload_buffer = message.payload_buffer();

  MessageHeader* header = reinterpret_cast<MessageHeader*>(payload_buffer.data());
  printf("+------- Message Header -------+\n");
  printf("| int size = %d\n", header->size);
  switch (header->type) {
    case MessageType::SEND_INVITATION:
      printf("| MessageType type = SEND_INVITATION\n");
      break;
    case MessageType::ACCEPT_INVITATION:
      printf("| MessageType type = ACCEPT_INVITATION\n");
      break;
    case MessageType::USER_MESSAGE:
      printf("| MessageType type = USER_MESSAGE\n");
      break;
  }
  printf("| int user_message_id = %d\n", header->user_message_id);
  printf("+-------- Message Body --------+\n");

  printf("|");
  for (size_t i = sizeof(MessageHeader); i < payload_buffer.size(); ++i) {
    printf("%02x ", payload_buffer[i]);
  }
  printf("\n");

  printf("+-------- End Message --------+\n");
}

}; // namespace

Channel::Channel(int fd, Delegate* delegate) : SocketReader(fd), delegate_(delegate) {
  CHECK_ON_THREAD(base::ThreadType::IO);
}

void Channel::Start() {
  CHECK_ON_THREAD(base::ThreadType::IO);
  auto io_task_loop =
    std::static_pointer_cast<base::TaskLoopForIO>(base::GetIOThreadTaskLoop());
  CHECK(io_task_loop);
  io_task_loop->WatchSocket(this);
}

void Channel::SetRemoteNodeName(const std::string& name) {
  CHECK_ON_THREAD(base::ThreadType::IO);
  remote_node_name_ = name;
}

void Channel::SendInvitation(std::string inviter_name, std::string intended_endpoint_name, std::string intended_endpoint_peer_name) {
  CHECK_ON_THREAD(base::ThreadType::IO);
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
  CHECK_ON_THREAD(base::ThreadType::IO);
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
  CHECK_ON_THREAD(base::ThreadType::IO);
  PrintFullMessageContents(message);

  std::vector<char>& payload_buffer = message.payload_buffer();
  write(fd_, payload_buffer.data(), payload_buffer.size());
}

void Channel::OnCanReadFromSocket() {
  CHECK_ON_THREAD(base::ThreadType::IO);
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
  CHECK_GE(header->size, full_message_buffer.size());
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

  PrintFullMessageContents(message);

  CHECK(delegate_);
  delegate_->OnReceivedMessage(std::move(message));
}

}; // namespace mage
