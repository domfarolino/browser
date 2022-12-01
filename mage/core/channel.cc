#include "mage/core/channel.h"

#include <stdio.h>
#include <sys/socket.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/threading/thread_checker.h" // for CHECK_ON_THREAD().
#include "mage/core/core.h"
#include "mage/core/endpoint.h"

namespace mage {

namespace {

// Helper function to print the full human-readable contents of a mage::Message.
void PrintFullMessageContents(Message& message) {
  // CHECK_ON_THREAD(base::ThreadType::UI);

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
  char target_endpoint_buffer[kIdentifierSize + 1];
  memcpy(target_endpoint_buffer, header->target_endpoint, kIdentifierSize);
  target_endpoint_buffer[kIdentifierSize] = '\0';
  printf("| str target_endpoint = %s \n", target_endpoint_buffer);
  printf("| Pointer<ArrayHeader<EndpointDescriptor>>->num_endpoints_in_message = %d \n", message.NumberOfHandles());
  printf("+-------- Message Body --------+\n");

  printf("|");
  for (size_t i = sizeof(MessageHeader); i < payload_buffer.size(); ++i) {
    printf("%02x ", payload_buffer[i]);
  }
  printf("\n");

  printf("+-------- End Message --------+\n");
}

}; // namespace

Channel::Channel(int fd, Delegate* delegate) :
    SocketReader(fd),
    delegate_(delegate),
    io_task_loop_(*std::static_pointer_cast<base::TaskLoopForIO>(
      base::GetIOThreadTaskLoop())) {
  CHECK_ON_THREAD(base::ThreadType::UI);
}

Channel::~Channel() {
  CHECK_ON_THREAD(base::ThreadType::UI);
  io_task_loop_.UnwatchSocket(this);
}

void Channel::Start() {
  CHECK_ON_THREAD(base::ThreadType::UI);
  io_task_loop_.WatchSocket(this);
}

void Channel::SetRemoteNodeName(const std::string& name) {
  CHECK_ON_THREAD(base::ThreadType::UI);
  remote_node_name_ = name;
}

void Channel::SendInvitation(std::string inviter_name,
                             std::string intended_endpoint_peer_name) {
  CHECK_ON_THREAD(base::ThreadType::UI);
  Message message(MessageType::SEND_INVITATION);
  MessageFragment<SendInvitationParams> params(message);
  params.Allocate();

  // Serialize inviter name.
  memcpy(params.data()->inviter_name, inviter_name.c_str(),
         inviter_name.size());

  // Serialize temporary remote node name.
  memcpy(params.data()->temporary_remote_node_name, remote_node_name_.c_str(),
         remote_node_name_.size());

  // Serialize intended endpoint peer name.
  memcpy(params.data()->intended_endpoint_peer_name,
         intended_endpoint_peer_name.c_str(),
         intended_endpoint_peer_name.size());

  message.FinalizeSize();
  SendMessage(std::move(message));
}

void Channel::SendAcceptInvitation(std::string temporary_remote_node_name,
                                   std::string actual_node_name,
                                   std::string accept_invitation_endpoint_name) {
  // CHECK(IsOnIOThread());
  Message message(MessageType::ACCEPT_INVITATION);
  MessageFragment<SendAcceptInvitationParams> params(message);
  params.Allocate();

  // Serialize temporary node name.
  memcpy(params.data()->temporary_remote_node_name,
         temporary_remote_node_name.c_str(), temporary_remote_node_name.size());

  // Serialize actual node name.
  memcpy(params.data()->actual_node_name, actual_node_name.c_str(),
         actual_node_name.size());

  // Serialize actual node name.
  memcpy(params.data()->accept_invitation_endpoint_name, accept_invitation_endpoint_name.c_str(),
         accept_invitation_endpoint_name.size());

  message.FinalizeSize();
  SendMessage(std::move(message));
}

void Channel::SendMessage(Message message) {
  printf("\n\nChannel::SendMessage(): getpid(): %d, fd_: %d\n", getpid(), fd_);
  // CHECK_ON_THREAD(base::ThreadType::UI);
  PrintFullMessageContents(message);

  std::vector<char>& payload_buffer = message.payload_buffer();
  CHECK_EQ(message.Size(), (int)payload_buffer.size());
  int rv = write(fd_, payload_buffer.data(), payload_buffer.size());
  CHECK_EQ(rv, message.Size());
}

void Channel::OnCanReadFromSocket() {
  printf("\n\nChannel::OnCanReadFromSocket() getpid: %d\n", getpid());
  CHECK_ON_THREAD(base::ThreadType::IO);
  std::vector<char> full_message_buffer;

  // Read the message header.
  {
    size_t header_size = sizeof(MessageHeader);
    std::vector<char> buffer(header_size);
    struct iovec iov = {buffer.data(), header_size};
    char cmsg_buffer[CMSG_SPACE(header_size)];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buffer;
    msg.msg_controllen = sizeof(cmsg_buffer);

    size_t rv = recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    CHECK_EQ(rv, header_size);
    full_message_buffer = std::move(buffer);
  }

  // Pull out the message header.
  int total_message_size = reinterpret_cast<MessageHeader*>(full_message_buffer.data())->size;
  full_message_buffer.resize(total_message_size);

  MessageHeader* header = reinterpret_cast<MessageHeader*>(full_message_buffer.data());
  CHECK_EQ(header->size, (int)full_message_buffer.size());

  // Read the message body.
  {
    size_t body_size = header->size - sizeof(MessageHeader);
    std::vector<char> buffer(body_size);
    struct iovec iov = {buffer.data(), body_size};
    char cmsg_buffer[CMSG_SPACE(body_size)];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buffer;
    msg.msg_controllen = sizeof(cmsg_buffer);

    size_t rv = recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    CHECK_EQ(rv, body_size);
    memcpy(full_message_buffer.data() + sizeof(MessageHeader), buffer.data(), body_size);
  }

  Message message(header->type);
  message.ConsumeBuffer(std::move(full_message_buffer));

  printf("Channel::OnCanReadFromSocket() [getpid(): %d] message contents:\n", getpid());
  PrintFullMessageContents(message);

  CHECK(delegate_);
  delegate_->OnReceivedMessage(std::move(message));
}

}; // namespace mage
