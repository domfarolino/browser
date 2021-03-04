#include <assert.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/threading/simple_thread.h"
#include "gtest/gtest.h"

namespace base {
namespace {

// These are messages that will be written to an underlying buffer via a file
// descriptor. For simplicity, their length should all be |kMessageLength|.
static const std::string kFirstMessage =      "First message ";
static const std::string kSecondMessage =     "Second message";
static const std::string kThirdMessage =      "Third message ";
static const std::string kFourthMessage =     "Fourth message";
static const int kMessageLength = 14;

// With this test base class, all tests and assertions will be running on a
// thread on which a |TaskLoopForIO| is bound. The tests may create and use
// other threads to asynchronously interact with the IO task loop on the main
// thread. In these cases, it will be useful for the tests to call
// |task_loop_for_io_->Run()| to wait for tasks and events to be posted.
class TaskLoopForIOTestBase : public testing::Test {
 public:
  void SetUp() {
    task_loop_for_io_ = std::static_pointer_cast<base::TaskLoopForIO>(
                          base::TaskLoop::Create(base::ThreadType::IO));

    // Setup and entangle two file descriptors to/from which tests can read and
    // write.
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_), 0);
    EXPECT_EQ(fcntl(fds_[0], F_SETFL, O_NONBLOCK), 0);
    EXPECT_EQ(fcntl(fds_[1], F_SETFL, O_NONBLOCK), 0);

    num_tasks_ran_ = 0;
    expected_message_count_ = 0;
    messages_read_.clear();
  }

  void TearDown() {
    task_loop_for_io_.reset();

    EXPECT_EQ(close(fds_[0]), 0);
    EXPECT_EQ(close(fds_[1]), 0);
  }

  std::shared_ptr<base::TaskRunner> GetTaskRunner() {
    return task_loop_for_io_->GetTaskRunner();
  }

  void RunTaskAndQuit() {
    num_tasks_ran_++;
    task_loop_for_io_->Quit();
  }

  void OnMessageRead(std::string message) {
    messages_read_.push_back(message);
    if (messages_read_.size() == expected_message_count_)
      task_loop_for_io_->Quit();
  }

  void SetExpectedMessageCount(int count) {
    expected_message_count_ = count;
  }

 protected:
  int fds_[2];
  std::shared_ptr<base::TaskLoopForIO> task_loop_for_io_;

  int num_tasks_ran_;
  std::vector<std::string> messages_read_;

 private:
  int expected_message_count_;
};

class TestSocketReader : public base::TaskLoopForIO::SocketReader {
 public:
  using MessageCallback = std::function<void(std::string)>;
  TestSocketReader(int fd): base::TaskLoopForIO::SocketReader::SocketReader(fd){}

  void SetCallback(MessageCallback callback) {
    callback_ = std::move(callback);
  }

  void OnCanReadFromSocket() override {
    char buffer[kMessageLength];
    struct iovec iov = {buffer, kMessageLength};
    char cmsg_buf[CMSG_SPACE(kMessageLength)];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    std::string message(buffer, buffer + kMessageLength);
    callback_(message);
  }

 private:
  MessageCallback callback_;
};

void WriteMessagesToFDWithDelay(int fd, std::vector<std::string> messages, int delay_ms) {
  if (delay_ms) {
    base::Thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }

  for (const std::string& message : messages) {
    write(fd, message.data(), message.size());
  }
}

TEST_F(TaskLoopForIOTestBase, BasicTaskPosting) {
  base::SimpleThread simple_thread(&TaskLoopForIOTestBase::RunTaskAndQuit,
                                   this);
  task_loop_for_io_->Run();
  EXPECT_EQ(num_tasks_ran_, 1);
}

TEST_F(TaskLoopForIOTestBase, BasicSocketReading) {
  SetExpectedMessageCount(1);

  TestSocketReader* socket_reader = new TestSocketReader(fds_[0]);
  TestSocketReader::MessageCallback callback =
    std::bind(&TaskLoopForIOTestBase::OnMessageRead, this,
              std::placeholders::_1);
  socket_reader->SetCallback(std::move(callback));
  task_loop_for_io_->WatchSocket(fds_[0], socket_reader);

  std::vector<std::string> messages_to_write = {kFirstMessage};
  // TODO(domfarolino): base::SimpleThread has a bug where std::ref is required
  // below.
  base::SimpleThread simple_thread(WriteMessagesToFDWithDelay,
                                   std::ref(fds_[1]),
                                   std::ref(messages_to_write), 0);
  task_loop_for_io_->Run();
  EXPECT_EQ(messages_read_.size(), 1);
  EXPECT_EQ(messages_read_[0], kFirstMessage);
}

// Just like the test above, but we write to the socket before the task loop is
// actually aware of the file descriptor to listen from. We're testing that we
// can read writes that were queued before we started listening.
TEST_F(TaskLoopForIOTestBase, WriteToSocketBeforeListening) {
  SetExpectedMessageCount(1);

  std::vector<std::string> messages_to_write = {kFirstMessage};
  // TODO(domfarolino): base::SimpleThread has a bug where std::ref is required
  // below.
  base::SimpleThread simple_thread(WriteMessagesToFDWithDelay,
                                   std::ref(fds_[1]),
                                   std::ref(messages_to_write), 0);

  // TODO(domfarolino): We shouldn't sleep for a hard-coded amount of time like
  // we are below. Instead we should consider giving TaskLoop the ability to
  // expose a quit closure and re-run itself. That way we can be notified when
  // an event happens, and respond by e.g., registering the TestSocketReader
  // below and re-running the loop to listen for messages etc.
  // Sleep, giving the thread above time to write to the file descriptor that we
  // haven't started listening to.
  base::Thread::sleep_for(std::chrono::milliseconds(400));

  TestSocketReader* socket_reader = new TestSocketReader(fds_[0]);
  TestSocketReader::MessageCallback callback =
    std::bind(&TaskLoopForIOTestBase::OnMessageRead, this,
              std::placeholders::_1);
  socket_reader->SetCallback(std::move(callback));
  task_loop_for_io_->WatchSocket(fds_[0], socket_reader);

  task_loop_for_io_->Run();
  EXPECT_EQ(messages_read_.size(), 1);
  EXPECT_EQ(messages_read_[0], kFirstMessage);
}

TEST_F(TaskLoopForIOTestBase, QueueingMessagesOnMultipleSockets) {
  SetExpectedMessageCount(4);

  // Set up the first SocketReader.
  TestSocketReader* reader_1 = new TestSocketReader(fds_[0]);
  TestSocketReader::MessageCallback callback =
    std::bind(&TaskLoopForIOTestBase::OnMessageRead, this,
              std::placeholders::_1);
  reader_1->SetCallback(std::move(callback));
  task_loop_for_io_->WatchSocket(fds_[0], reader_1);

  // Set up the second SocketReader.
  int moreFds[2];
  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, moreFds), 0);
  EXPECT_EQ(fcntl(moreFds[0], F_SETFL, O_NONBLOCK), 0);
  EXPECT_EQ(fcntl(moreFds[1], F_SETFL, O_NONBLOCK), 0);
  TestSocketReader* reader_2 = new TestSocketReader(moreFds[0]);
  reader_2->SetCallback(std::move(callback));
  task_loop_for_io_->WatchSocket(moreFds[0], reader_2);

  // TODO(domfarolino): base::SimpleThread has a bug where std::ref is required
  // below.
  // Two separate threads each queue a message
  std::vector<std::string> messages_1 = {kFirstMessage, kSecondMessage};
  std::vector<std::string> messages_2 = {kThirdMessage, kFourthMessage};
  base::SimpleThread thread1(WriteMessagesToFDWithDelay, std::ref(fds_[1]),
                             std::ref(messages_1), 200);
  base::SimpleThread thread2(WriteMessagesToFDWithDelay, std::ref(moreFds[1]),
                             std::ref(messages_2), 600);

  // Post a task on the task loop that will block the loop for 1 second while
  // the above threads queue messages, then immediately run the queue so that
  // task is run.
  task_loop_for_io_->PostTask(std::bind([](){
    base::Thread::sleep_for(std::chrono::milliseconds(1000));
  }));
  task_loop_for_io_->Run();

  EXPECT_EQ(messages_read_.size(), 4);
  // The ordering looks funky here because our task queue services each file
  // descriptor evenly, instead of starving one of them by reading all of the
  // other's events.
  EXPECT_EQ(messages_read_[0], kFirstMessage);
  EXPECT_EQ(messages_read_[1], kThirdMessage);
  EXPECT_EQ(messages_read_[2], kSecondMessage);
  EXPECT_EQ(messages_read_[3], kFourthMessage);

  EXPECT_EQ(close(moreFds[0]), 0);
  EXPECT_EQ(close(moreFds[1]), 0);
}

// TODO: Test the following cases:
//   - Multiple tasks are posted while a task is already running: test that when
//     the currently-running task is done, the loop correctly resumes and serves
//     all posted tasks
//   - Same thing as above but interpolated with message writes as well (see
//     below function that used to do this in a non-test environment).

/*
// A helper that runs on another thread and writes to a given file descriptor.
void WriteToFDFromSimpleThread(int fd,
                               std::shared_ptr<base::TaskRunner> task_runner) {
  // 1.) Write a message.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  std::string first_message = kFirstMessage; // Character for padding.
  write(fd, first_message.data(), first_message.size());

  // 2.) Post a task.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  task_runner->PostTask(std::bind([](){
    printf("First task is running\n");
  }));

  // 3.) Write another message.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  std::string second_message = kSecondMessage;
  write(fd, second_message.data(), second_message.size());

  // 4.) Post another task.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  task_runner->PostTask(std::bind([](){
    printf("Second task is running\n");
  }));
}
*/

}; // namespace
}; // namespace base
