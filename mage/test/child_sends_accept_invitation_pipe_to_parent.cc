#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/threading/thread_checker.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/test/magen/first_interface.magen.h"  // Generated.
#include "mage/test/magen/second_interface.magen.h"  // Generated.

void OnInvitationAccepted(mage::MageHandle remote_handle) {
  CHECK_ON_THREAD(base::ThreadType::UI);
  mage::Remote<magen::FirstInterface> remote;
  remote.Bind(remote_handle);

  std::vector<mage::MageHandle> pipes = mage::Core::CreateMessagePipes();
  remote->SendSecondInterfaceReceiver(pipes[1]);

  mage::Remote<magen::SecondInterface> second_remote;
  second_remote.Bind(pipes[0]);
  // TODO(domfarolino): Sending an endpoint that is bound shouldn't be
  // allowed.
  second_remote->SendReceiverForThirdInterface(remote_handle);
}

int main(int argc, char** argv) {
  std::shared_ptr<base::TaskLoop> main_thread = base::TaskLoop::Create(base::ThreadType::UI);
  base::Thread io_thread(base::ThreadType::IO);
  io_thread.Start();
  io_thread.GetTaskRunner()->PostTask(main_thread->QuitClosure());
  main_thread->Run();

  mage::Core::Init();

  CHECK_EQ(argc, 2);
  int fd = std::stoi(argv[1]);
  mage::Core::AcceptInvitation(fd, &OnInvitationAccepted);

  main_thread->Run();
  return 0;
}