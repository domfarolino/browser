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

int main(int argc, char** argv) {
  std::shared_ptr<base::TaskLoop> main_thread = base::TaskLoop::Create(base::ThreadType::UI);
  base::Thread io_thread;
  mage::Core::Init();

  int fd = std::stoi(argv[1]);
  mage::MageHandle message_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(fd, [&](){
      // Asynchronously quit the test now that we know that below message, that
      // was queued synchronously, has been sent to the remote process.
      base::GetCurrentThreadTaskLoop()->Quit();
    });
  mage::Remote<magen::TestInterface> remote;
  remote.Bind(message_pipe);
  remote->Method1(1, .5, "message");
  remote->SendMoney(1000, "JPY");

  main_thread->Run();
  return 0;
}
