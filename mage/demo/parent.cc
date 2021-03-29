#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <memory>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/bindings/receiver.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/demo/magen/demo.magen.h" // Generated.

class DemoImpl : public magen::Demo {
 public:
  DemoImpl(mage::MageHandle handle) {
    printf("DemoImpl::ctor binding to handle\n");
    receiver_.Bind(handle, this);
  }

  // magen::Demo implementation.
  void Method1(int a, std::string b, std::string c) override {
    printf("DemoImpl::Method1()\n");
    printf("a: %d, b: %s, c: %s\n", a, b.c_str(), c.c_str());
  }

 private:
  mage::Receiver<magen::Demo> receiver_;
};

int main() {
  printf("-------- Parent process --------\n");
  int fds[2];
  CHECK_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  CHECK_EQ(fcntl(fds[0], F_SETFL, O_NONBLOCK), 0);
  CHECK_EQ(fcntl(fds[1], F_SETFL, O_NONBLOCK), 0);

  pid_t rv = fork();
  if (rv == 0) { // Child.
    rv = execl("./bazel-bin/mage/demo/child", "--mage-socket=", std::to_string(fds[1]).c_str());
  }

  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  mage::Core::Init();

  // Spin up a new process, and have it access fds[1].
  mage::MageHandle local_message_pipe =
    mage::Core::SendInvitationToTargetNodeAndGetMessagePipe(fds[0]);
  printf("Parent local_message_pipe for receiver: %d\n", local_message_pipe);

  DemoImpl* demo = new DemoImpl(local_message_pipe);

  task_loop->Run();
  return 0;
}
