#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/demo/magen/demo.magen.h"  // Generated.

void OnInvitationAccepted(mage::MageHandle handle) {
  printf("OnInvitationAccepted: %d\n", handle);
  mage::Remote<magen::Demo> remote;
  remote.Bind(handle);
  remote->Method1(1, "this", "that");
}

int main(int argc, char** argv) {
  printf("-------- Child process --------\n");
  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  mage::Core::Init();

  int fd = std::stoi(argv[1]);
  printf("File desc to read from is: %d\n", fd);

  mage::Core::AcceptInvitation(fd, &OnInvitationAccepted);

  task_loop->Run();
  return 0;
}
