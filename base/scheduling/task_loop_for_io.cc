#include "base/scheduling/task_loop_for_io.h"

namespace base {

void TaskLoopForIO::Run() {
  while (true) {
    int rv = kevent64(kqueue_fd_, nullptr, 0, events_.data(), events_.size(), 0, nullptr);
    CHECK_GEQ(rv, 0);

    if (quit_)
      break;

    mutex_.lock();
    CHECK(queue_.size());
    Callback cb = std::move(queue_.front());
    queue_.pop();
    mutex_.unlock();

    ExecuteTask(std::move(cb));
  }
}

// Can be called from any thread.
void TaskLoopForIO::PostTask(Callback cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  ///// CHROMIUM SRC
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  message.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  message.header.msgh_remote_port = wakeup_;
  kern_return_t kr = mach_msg_send(&message.header);
  //// end CHROMIUM SRC
  CHECK_EQ(kr, KERN_SUCCESS);
  mutex_.unlock();
}

void TaskLoopForIO::Quit() {
  quit_ = true;
  mutex_.lock();
  ///// CHROMIUM SRC
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  message.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  message.header.msgh_remote_port = wakeup_;
  kern_return_t kr = mach_msg_send(&message.header);
  //// end CHROMIUM SRC
  CHECK_EQ(kr, KERN_SUCCESS);
  mutex_.unlock();
}

}; // namespace base
