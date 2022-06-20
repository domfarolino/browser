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
#include "examples/mage/magen/child_process_2.magen.h" // Generated

int main() {
  // Set up the main thread (this thread) to have a `base::TaskLoop` bound to it
  // that we can run and process tasks on, as well as a separate IO thread.
  std::shared_ptr<base::TaskLoop> main_thread =
      base::TaskLoop::Create(base::ThreadType::UI);
  base::Thread io_thread(base::ThreadType::IO);
  io_thread.Start();
  io_thread.GetTaskRunner()->PostTask(main_thread->QuitClosure());
  // Wait until `io_thread` has asynchronously started. This blocks the main
  // thread (via its `TaskLoop` until the IO thread is ready to handle tasks
  // itself.
  main_thread->Run();

  // Set up the sockets for the parent and child process to communicate over.
  int sockets[2];
  CHECK_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  CHECK_EQ(fcntl(sockets[0], F_SETFL, O_NONBLOCK), 0);
  CHECK_EQ(fcntl(sockets[1], F_SETFL, O_NONBLOCK), 0);

  // Launch the child process.
  pid_t child_pid = fork();
  if (child_pid == 0) { // if (child process) {
    std::string remote_socket_as_string = std::to_string(sockets[1]);
    execl("./bazel-bin/examples/mage/mage_child", "--mage-socket=",
          remote_socket_as_string.c_str(), NULL);
  }

  // The actual interesting stuff:
  mage::Core::Init();
  // Once we send the invitation, we synchronously get a handle that we can use
  // in a `mage::Remote`. We can synchronously start sending messages over the
  // remote and they'll magically end up on whatever receiver gets bound to the
  // other end.
  mage::MageHandle message_pipe =
      mage::Core::SendInvitationAndGetMessagePipe(sockets[0]);
  mage::Remote<magen::ChildProcess> remote;
  remote.Bind(message_pipe);
  remote->PrintMessage("Hello from the parent process!!!");

  std::vector<mage::MageHandle> mage_handles = mage::Core::CreateMessagePipes();
  // TODO(domfarolino): Figure out why we need this and get rid of it.
  base::Thread::sleep_for(std::chrono::milliseconds(1000));
  remote->PassHandle(mage_handles[1]);

  mage::Remote<magen::ChildProcess2> remote_2;
  remote_2.Bind(mage_handles[0]);
  remote_2->PrintMessage2("This is the second message from the parent!");

  base::Thread::sleep_for(std::chrono::milliseconds(2000));
  int tmp;
  std::cout << "Pess any button and hit enter to kill the parent and child "
            << "processes";
  std::cin >> tmp;
  kill(child_pid, SIGTERM);
  CHECK_EQ(close(sockets[0]), 0);
  CHECK_EQ(close(sockets[1]), 0);
  return 0;
}
