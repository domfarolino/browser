#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>

#include <iostream>

#include "base/check.h"
#include "base/scheduling/task_loop.h"
#include "base/threading/thread.h"
#include "mage/core/core.h"
#include "mage/bindings/remote.h"

#include "examples/mage/magen/child_process.magen.h" // Generated

int main() {
  std::shared_ptr<base::TaskLoop> main_thread =
      base::TaskLoop::Create(base::ThreadType::UI);
  base::Thread io_thread(base::ThreadType::IO);
  io_thread.Start();
  // Wait until `io_thread` has asynchronously started.
  io_thread.GetTaskRunner()->PostTask(main_thread->QuitClosure());
  main_thread->Run();

  // Set up the sockets for the parent and child process to communicate over.
  int sockets[2];
  CHECK_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  CHECK_EQ(fcntl(sockets[0], F_SETFL, O_NONBLOCK), 0);
  CHECK_EQ(fcntl(sockets[1], F_SETFL, O_NONBLOCK), 0);

  pid_t child_pid = fork();
  if (child_pid == 0) { // if (child process)
    std::string remote_socket_as_string = std::to_string(sockets[1]);
    execl("./bazel-bin/examples/mage/mage_child", "--mage-socket=", remote_socket_as_string.c_str(), NULL);
  }

  // THE ACTUAL INTERESTING STUFF:
  mage::Core::Init();
  mage::MageHandle message_pipe = mage::Core::SendInvitationAndGetMessagePipe(sockets[0]);
  mage::Remote<magen::ChildProcess> remote;
  remote.Bind(message_pipe);
  remote->PrintMessage("Hello from the parent process!!!");

  int tmp;
  std::cout << "When you see the message printed out by the child process, press any button and hit enter";
  std::cin >> tmp;
  kill(child_pid, SIGTERM);
  CHECK_EQ(close(sockets[0]), 0);
  CHECK_EQ(close(sockets[1]), 0);
  return 0;
}
