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
#include "mage/bindings/receiver.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/test/magen/callback_interface.magen.h"  // Generated.
#include "mage/test/magen/first_interface.magen.h"  // Generated.
#include "mage/test/magen/second_interface.magen.h"  // Generated.

// The correct sequence of message that this binary will receive from the parent
// process (test runner) is:
//   1.) FirstInterface::SendString()
//   2.) FirstInterface::SendHandles()
//   3.) SecondInterface::SendString()
//   5.) SecondInterface::SendStringAndNotifyDone()

bool first_interface_received_send_string = false;
bool first_interface_received_send_handles = false;
bool second_interface_received_send_string = false;

class SecondInterfaceImpl final : public magen::SecondInterface {
 public:
  SecondInterfaceImpl(mage::MageHandle receiver, mage::MageHandle callback_handle) {
    receiver_.Bind(receiver, this);
    callback_.Bind(callback_handle);
  }

  void SendString(std::string message) {
    CHECK(first_interface_received_send_string);
    CHECK(first_interface_received_send_handles);
    CHECK(!second_interface_received_send_string);
    second_interface_received_send_string = true;

    CHECK_EQ(message, "Message for SecondInterface");
    printf("\033[34;1mSecondInterfaceImpl received message: %s\033[0m\n",
           message.c_str());
    // This will notify the parent process that we've received all of the
    // messages, so it can tear things down.
    callback_->NotifyDone();
  }

  void SendStringAndNotifyDone(std::string message) {
    CHECK_EQ(message, "Parent string (2)");
    callback_->NotifyDone();

    // This allows the loop, and therefore this process, to terminate.
    base::GetCurrentThreadTaskLoop()->Quit();
  }

 private:
  mage::Receiver<magen::SecondInterface> receiver_;
  mage::Remote<magen::CallbackInterface> callback_;
};
std::unique_ptr<SecondInterfaceImpl> global_second_interface;

class FirstInterfaceImpl final : public magen::FirstInterface {
 public:
  FirstInterfaceImpl(mage::MageHandle handle) {
    receiver_.Bind(handle, this);
  }

  void SendString(std::string message) {
    CHECK_EQ(message, "Message for FirstInterface");
    printf("\033[34;1mFirstInterfaceImpl received message: %s\033[0m\n",
           message.c_str());
    first_interface_received_send_string = true;
  }

  void SendHandles(mage::MageHandle second_interface,
                   mage::MageHandle callback_handle) {
    CHECK(first_interface_received_send_string);
    CHECK(!first_interface_received_send_handles);
    first_interface_received_send_handles = true;

    global_second_interface =
        std::make_unique<SecondInterfaceImpl>(second_interface, callback_handle);
  }

 private:
  mage::Receiver<magen::FirstInterface> receiver_;
};
std::unique_ptr<FirstInterfaceImpl> global_first_interface;

void OnInvitationAccepted(mage::MageHandle receiver_handle) {
  CHECK_ON_THREAD(base::ThreadType::UI);
  global_first_interface = std::make_unique<FirstInterfaceImpl>(receiver_handle);
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
