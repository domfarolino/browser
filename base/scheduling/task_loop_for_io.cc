#include <mach/mach.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/event.h>

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
  // Send an empty message to |wakeup_|. There are three things that can happen here:
  //   1.) This is being called from another thread, and the task loop's thread
  //       is sleeping. In this case, the message will wake up the sleeping
  //       thread and it will execute the task we just pushed to the queue.
  //   2.) This is being called from another thread, and the task loop's thread
  //       is not sleeping, but running another task. In this case, the message
  //       will get pushed to the kernel queue to ensure that the thread doesn't
  //       sleep on its next iteration of its running loop, and instead executes
  //       the next task (the task we just posted if there are no others in
  //       front of it in the queue)
  //   3.) This is being called from the thread that the task loop is running
  //       on. In that case, we're in the middle of a task right now, which is
  //       the same thing as (2) above.
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  message.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  message.header.msgh_remote_port = wakeup_;
  kern_return_t kr = mach_msg_send(&message.header);

  CHECK_EQ(kr, KERN_SUCCESS);
  mutex_.unlock();
}

void TaskLoopForIO::Quit() {
  quit_ = true;
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  message.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  message.header.msgh_remote_port = wakeup_;
  kern_return_t kr = mach_msg_send(&message.header);

  CHECK_EQ(kr, KERN_SUCCESS);
}

}; // namespace base
