#include <mach/mach.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/socket.h>

#include <vector>

#include "base/scheduling/task_loop_for_io_mac.h"

namespace base {

void TaskLoopForIOMac::Run() {
  while (true) {
    // The last task that ran may have introduced a new |SocketReader| that
    // increased our |event_count_|, and thus increases the number of event
    // filters that we care about listening for. In orer for the kqueue to
    // notify us of potentially multiple different kinds of events in one go, we
    // expand our |events_| vector, so kevent64 and push multiple events (with
    // unique filters) onto |events_|.
    events_.resize(event_count_);
    int rv = kevent64(kqueue_, nullptr, 0, events_.data(), events_.size(), 0,
                      nullptr);

    // At this point we had to have read at least one event from the kernel.
    CHECK_GEQ(rv, 1);

    if (quit_)
      break;

    for (int i = 0; i < rv; ++i) {
      auto* event = &events_[i];

      if (event->filter == EVFILT_READ) {
        int fd = event->ident;
        auto* socket_reader = async_socket_readers_[fd];
        CHECK(socket_reader);
        socket_reader->OnCanReadFromSocket();
      } else if (event->filter == EVFILT_MACHPORT) {
        mutex_.lock();
        CHECK(queue_.size());
        Callback cb = std::move(queue_.front());
        queue_.pop();
        mutex_.unlock();

        ExecuteTask(std::move(cb));
      } else {
        NOTREACHED();
      }
    } // for.

  } // while (true).
}

void TaskLoopForIOMac::WatchSocket(int fd, SocketReader* socket_reader) {
  std::vector<kevent64_s> events;

  kevent64_s new_event{};
  new_event.ident = fd;
  new_event.flags = EV_ADD;
  new_event.filter = EVFILT_READ;
  events.push_back(new_event);

  // Invoke kevent64 not to listen to events, but to supply a changelist of
  // event filters that we're interested in being notified about from the
  // kernel.
  int rv = kevent64(kqueue_, events.data(), events.size(), nullptr, 0, 0, nullptr);
  CHECK_GEQ(rv, 0);

  async_socket_readers_[fd] = socket_reader;

  event_count_++;
}

// Can be called from any thread.
void TaskLoopForIOMac::PostTask(Callback cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  // Send an empty message to |wakeup_|. There are three things that can happen here:
  //   1.) This is being called from another thread, and the task loop's thread
  //       is sleeping. In this case, the message will wake up the sleeping
  //       thread and it will execute the task we just pushed to the queue.
  //   2.) This is being called from another thread, and the task loop's thread
  //       is not sleeping, but running another task. In this case, the message
  //       will get pushed to the kernel queue to ensure that the thread doesn't
  //       sleep on its next iteration of its run loop, and instead executes the
  //       next task in the queue (possible this one, if there are no others
  //       before it).
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

void TaskLoopForIOMac::Quit() {
  mutex_.lock();
  quit_ = true;
  mutex_.unlock();
  // See documentation in |PostTask()|.
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  message.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  message.header.msgh_remote_port = wakeup_;

  kern_return_t kr = mach_msg_send(&message.header);
  CHECK_EQ(kr, KERN_SUCCESS);
}

}; // namespace base
