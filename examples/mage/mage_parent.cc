#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>

#include <iostream>

#include "base/check.h"
#include "base/scheduling/task_loop.h"
#include "base/threading/thread.h"
#include "mage/core/core.h"
#include "mage/bindings/remote.h"
#include "mage/bindings/receiver.h"

#include "examples/mage/magen/child_process.magen.h" // Generated
#include "examples/mage/magen/child_process_2.magen.h" // Generated
#include "examples/mage/magen/parent_process.magen.h" // Generated

class ParentProcessImpl final : public magen::ParentProcess {
 public:
  ParentProcessImpl(mage::MessagePipe parent_receiver) {
    receiver_.Bind(parent_receiver, this);
  }

  // We receive this IPC when the child process is done receiving all messages
  // we send it. This is how we know how to quit our process and tear down the
  // child.
  void NotifyDone() {
    printf("\033[34;1mParentProcessImpl::NotifyDone() called from child\033[0m\n");
    base::GetCurrentThreadTaskLoop()->Quit();
  }

 private:
  mage::Receiver<magen::ParentProcess> receiver_;
};

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
  mage::MessagePipe message_pipe =
      mage::Core::SendInvitationAndGetMessagePipe(sockets[0]);
  mage::Remote<magen::ChildProcess> remote;
  remote.Bind(message_pipe);
  remote->PrintMessage("Hello from the parent process!!!");

  std::vector<mage::MessagePipe> child_process_2_handles = mage::Core::CreateMessagePipes();
  std::vector<mage::MessagePipe> parent_process_handles = mage::Core::CreateMessagePipes();
  remote->PassHandle(child_process_2_handles[1], parent_process_handles[1]);

  mage::Remote<magen::ChildProcess2> remote_2;
  remote_2.Bind(child_process_2_handles[0]);
  remote_2->PrintMessage2("This is the second message from the parent!");

  std::unique_ptr<ParentProcessImpl> parent(new ParentProcessImpl(parent_process_handles[0]));
  main_thread->Run();

  int tmp;
  std::cout << "Pess any button and hit enter to kill the parent and child "
            << "processes";
  std::cin >> tmp;
  kill(child_pid, SIGTERM);
  CHECK_EQ(close(sockets[0]), 0);
  CHECK_EQ(close(sockets[1]), 0);
  return 0;
}
