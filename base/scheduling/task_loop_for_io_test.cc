#include <assert.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>

#include "base/check.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/threading/simple_thread.h"
#include "gtest/gtest.h"

namespace base {
namespace {

// These are messages that will be written to an underlying buffer via a file
// descriptor. For simplicity, their length should all be |kMessageLength|.
static const char* kFirstMessage = "First message ";
static const char* kSecondMessage = "Second message";
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
    num_messages_read_ = 0;
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

  void OnMessageReadAndQuit(std::string message) {
    num_messages_read_++;
    last_message_read_ = message;
    task_loop_for_io_->Quit();
  }

 protected:
  int fds_[2];
  std::shared_ptr<base::TaskLoopForIO> task_loop_for_io_;

  int num_tasks_ran_;
  int num_messages_read_;
  std::string last_message_read_;
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

void WriteMessageToFDFromSimpleThread(int fd) {
  std::string first_message = kFirstMessage;
  write(fd, first_message.data(), first_message.size());
}

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

TEST_F(TaskLoopForIOTestBase, BasicTaskPosting) {
  base::SimpleThread simple_thread(&TaskLoopForIOTestBase::RunTaskAndQuit,
                                   this);
  task_loop_for_io_->Run();
  EXPECT_EQ(num_tasks_ran_, 1);
}

TEST_F(TaskLoopForIOTestBase, BasicSocketReading) {
  TestSocketReader* socket_reader = new TestSocketReader(fds_[0]);
  TestSocketReader::MessageCallback callback =
    std::bind(&TaskLoopForIOTestBase::OnMessageReadAndQuit, this,
              std::placeholders::_1);
  socket_reader->SetCallback(std::move(callback));
  task_loop_for_io_->WatchSocket(fds_[0], socket_reader);

  base::SimpleThread simple_thread(
    WriteMessageToFDFromSimpleThread, std::ref(fds_[1]));
  task_loop_for_io_->Run();
  EXPECT_EQ(num_messages_read_, 1);
  EXPECT_EQ(last_message_read_, kFirstMessage);
}

/*
TEST_F(TaskLoopForIOTestBase, BasicSocketReading2) {
  base::SimpleThread simple_thread(WriteToFDFromSimpleThread, std::ref(fds_[1]),
                                   task_loop_for_io_->GetTaskRunner());
  TestSocketReader* socket_reader = new TestSocketReader(fds_[0]);
  task_loop_for_io_->WatchSocket(fds_[0], socket_reader);
  task_loop_for_io_->Run();
}
*/

// TODO: Test the following cases:
//   - A thread writing to a file descriptor before we are formally listening to
//     it
//   - Multiple tasks are posted while a task is already running: test that when
//     the currently-running task is done, the loop correctly resumes and serves
//     all posted tasks
//   - Multiple writes are made a file descriptor that a given tests is
//     listening to, all while a long-running task is executing. Once the task
//     is finished executing, test that all writes are available to read (we
//     really care about testing that our TaskLoopForIO is not messing up and
//     concealing kernel events that are ready to be read, causing us to never
//     get to them.
//   - A test that combines the two above test cases to make sure that all posts
//     and socket reads are available, and not accidentally lost by our task
//     loop

/*

// Test 2
void CalledOnIOThread() {
  printf("|CalledOnIOThread()| was invoked on the IO thread!!\n");
}

int main() {
  // Set up a task loop on the current thread. There is no need to use
  // base::Thread for this, since we don't want to spin up a new physical
  // thread.
  auto task_loop = base::TaskLoop::Create(base::ThreadType::WORKER);

  base::Thread io_thread(base::ThreadType::IO);
  io_thread.Start();

  printf("Main thread is posting a task to the IO thread\n");
  io_thread.GetTaskRunner()->PostTask(&CalledOnIOThread);
  io_thread.GetTaskRunner()->PostTask(&CalledOnIOThread);
  io_thread.GetTaskRunner()->PostTask(&CalledOnIOThread);

  task_loop->Run();
  return 0;
}
*/

}; // namespace
}; // namespace base
