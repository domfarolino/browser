#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/threading/thread.h"
#include "mage/bindings/receiver.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"

#include "examples/mage/magen/child_process.magen.h"  // Generated.
#include "examples/mage/magen/child_process_2.magen.h"  // Generated.
#include "examples/mage/magen/parent_process.magen.h"  // Generated.

class ChildProcessImpl2 : public magen::ChildProcess2 {
 public:
  ChildProcessImpl2(mage::MageHandle message_pipe) {
    receiver_.Bind(message_pipe, this);
  }

  void PrintMessage2(std::string msg) override {
    printf("\033[34;1mChildProcessImpl2 is printing a message from the "
           "parent:\033[0m\n");
    printf("\033[34;1m%s\033[0m\n", msg.c_str());
  }

 private:
  mage::Receiver<magen::ChildProcess2> receiver_;
};
std::unique_ptr<ChildProcessImpl2> global_child_process_impl_2;

class ChildProcessImpl : public magen::ChildProcess {
 public:
  ChildProcessImpl(mage::MageHandle message_pipe) {
    receiver_.Bind(message_pipe, this);
  }

  void PrintMessage(std::string msg) override {
    printf("\033[34;1mChildProcessImpl is printing a message from the "
           "parent:\033[0m\n");
    printf("\033[34;1m%s\033[0m\n", msg.c_str());
  }

  void PassHandle(mage::MageHandle child_process_2_handle, mage::MageHandle parent_process_handle) override {
    printf("\033[34;1mChildProcessImpl::PassHandle\033[0m\n");
    global_child_process_impl_2 = std::make_unique<ChildProcessImpl2>(child_process_2_handle);
    mage::Remote<magen::ParentProcess> remote;
    remote.Bind(parent_process_handle);
    remote->NotifyDone();
  }

 private:
  mage::Receiver<magen::ChildProcess> receiver_;
};
std::unique_ptr<ChildProcessImpl> global_child_process_impl;

void OnInvitationAccepted(mage::MageHandle handle) {
  // Bind the `ChildProcessImpl` as the backing implementation, and let the main
  // thread keep running until we asynchronously start receiving messages
  global_child_process_impl = std::make_unique<ChildProcessImpl>(handle);
}

int main(int argc, char** argv) {
  std::shared_ptr<base::TaskLoop> main_thread =
      base::TaskLoop::Create(base::ThreadType::UI);
  base::Thread io_thread(base::ThreadType::IO);
  io_thread.Start();
  io_thread.GetTaskRunner()->PostTask(main_thread->QuitClosure());
  main_thread->Run();

  mage::Core::Init();

  CHECK_EQ(argc, 2);
  int socket = std::stoi(argv[1]);
  mage::Core::AcceptInvitation(socket, &OnInvitationAccepted);

  main_thread->Run();
  return 0;
}
