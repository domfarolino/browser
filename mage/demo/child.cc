#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop_for_io.h"
#include "mage/bindings/receiver.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/demo/magen/demo.magen.h"  // Generated.

class DemoImpl : public magen::Demo {
 public:
  DemoImpl(mage::MageHandle handle) {
    receiver_.Bind(handle, this);
    base::GetCurrentThreadTaskLoop()->Run();
  }

  // magen::Demo implementation.
  void Method1(int a, std::string b, std::string c) override {
    printf("DemoImpl::Method1()\n");
    printf("  a: %d, b: %s, c: %s\n", a, b.c_str(), c.c_str());
  }

 private:
  mage::Receiver<magen::Demo> receiver_;
};

void OnInvitationAccepted(mage::MageHandle message_pipe) {
  std::unique_ptr<DemoImpl> demo(new DemoImpl(message_pipe));
}

int main(int argc, char** argv) {
  printf("-------- Child process --------\n");
  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  mage::Core::Init();

  int fd = std::stoi(argv[1]);

  mage::Core::AcceptInvitation(fd, &OnInvitationAccepted);
  task_loop->Run();
  return 0;
}
