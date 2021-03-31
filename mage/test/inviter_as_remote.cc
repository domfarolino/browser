#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop_for_io.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/test/magen/test.magen.h"  // Generated.

void OnInvitationAccepted(mage::MageHandle handle) {
  mage::Remote<magen::TestInterface> remote;
  remote.Bind(handle);
  remote->Method1(1, .5, "message");

  // Stop the loop and kill the process.
  base::GetCurrentThreadTaskLoop()->Quit();
}

int main(int argc, char** argv) {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  mage::Core::Init();

  int fd = std::stoi(argv[1]);
  mage::MageHandle message_pipe = mage::Core::SendInvitationAndGetMessagePipe(fd);

  // Nasty sleep until we accept async invitations on both ends.
  base::Thread::sleep_for(std::chrono::milliseconds(1500));

  task_loop->PostTask([&](){

    task_loop->PostTask([&](){
      mage::Remote<magen::TestInterface> remote;
      remote.Bind(message_pipe);
      remote->Method1(1, .5, "message");
      // Stop the loop and kill the process.
      base::GetCurrentThreadTaskLoop()->Quit();
    });

  });

  task_loop->Run();
  return 0;
}
