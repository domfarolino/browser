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
  remote->SendMoney(1000, "JPY");

  // Stop the loop and kill the process.
  base::GetCurrentThreadTaskLoop()->Quit();
}

int main(int argc, char** argv) {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  mage::Core::Init();

  int fd = std::stoi(argv[1]);
  mage::Core::AcceptInvitation(fd, &OnInvitationAccepted);

  task_loop->Run();
  return 0;
}
