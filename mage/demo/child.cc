#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/demo/magen/demo.magen.h"  // Generated.
#include "mage/core/core.h"
#include "mage/core/handles.h"

class DemoImpl : public magen::Demo {
 public:
  DemoImpl() {
    printf("DemoImpl::ParentImpl\n");
  }

  void Method1() {
    printf("DemoImpl::Method1()\n");
  }
};

void OnInvitationAccepted(mage::MageHandle handle) {
  printf("OnInvitationAccepted: %d\n", handle);
}

int main(int argc, char** argv) {
  printf("-------- Child process --------\n");
  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  mage::Core::Init(std::static_pointer_cast<base::TaskLoopForIO>(task_loop).get());

  int fd = std::stoi(argv[1]);
  printf("File desc to read from is: %d\n", fd);

  mage::Core::AcceptInvitation(fd, &OnInvitationAccepted);

  task_loop->Run();
  return 0;
}
