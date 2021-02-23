#include <assert.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>

#include "base/check.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/threading/simple_thread.h"

class SocketReaderImpl : public base::TaskLoopForIO::SocketReader {
 public:
  SocketReaderImpl(int fd): base::TaskLoopForIO::SocketReader::SocketReader(fd) {}
  void OnCanReadFromSocket() {
    char* buf[25];
    struct iovec iov = {buf, 25};
    char cmsg_buf[CMSG_SPACE(25* sizeof(int))];
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    recvmsg(fd_, &msg, /*non blocking*/MSG_DONTWAIT);
    printf("  %s\n", buf);
  }
};

std::shared_ptr<base::TaskRunner> g_io_task_runner;

void WriteToFDFromSimpleThread(int fd) {
  // 1.) Write a message.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  printf("1.) |WriteToFDFromSimpleThread| is writing a message to IO thread\n");
  char* msg = "[READ] 1.) First message \0";
  write(fd, msg, strlen(msg));

  // 2.) Post a task.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  printf("2.) |WriteToFDFromSimpleThread| is posting a task to IO thread\n");
  g_io_task_runner->PostTask(std::bind([](){
    printf("  [TASK]: 2.) First task is running\n");
  }));

  // 3.) Write another message.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  printf("3.) |WriteToFDFromSimpleThread| is writing a message to IO thread\n");
  msg = "[READ] 3.) Second message\0";
  write(fd, msg, strlen(msg));

  // 4.) Post another task.
  base::Thread::sleep_for(std::chrono::milliseconds(300));
  printf("4.) |WriteToFDFromSimpleThread| is posting a task to IO thread\n");
  g_io_task_runner->PostTask(std::bind([](){
    printf("  [TASK]: 4.) Second task is running\n");
  }));
}

int main() {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  g_io_task_runner = task_loop->GetTaskRunner();

  int fds[2];
  CHECK_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  CHECK_EQ(fcntl(fds[0], F_SETFL, O_NONBLOCK), 0);
  CHECK_EQ(fcntl(fds[1], F_SETFL, O_NONBLOCK), 0);

  base::SimpleThread test(WriteToFDFromSimpleThread, std::ref(fds[1]));
  
  base::Thread::sleep_for(std::chrono::milliseconds(2000));

  SocketReaderImpl* socket_reader = new SocketReaderImpl(fds[0]);
  static_cast<base::TaskLoopForIO*>(task_loop.get())->WatchSocket(fds[0], socket_reader);

  task_loop->Run();
  return 0;
}
