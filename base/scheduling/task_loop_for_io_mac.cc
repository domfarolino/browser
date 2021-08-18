#include "base/scheduling/task_loop_for_io_mac.h"

#include <mach/mach.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/socket.h>

#include <vector>

namespace base {

TaskLoopForIOMac::TaskLoopForIOMac() : kqueue_(kqueue()) {
  kern_return_t kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE, &wakeup_);
  CHECK_EQ(kr, KERN_SUCCESS);

  kevent64_s event{};
  event.ident = wakeup_;
  event.filter = EVFILT_MACHPORT;
  event.flags = EV_ADD;
  event.fflags = MACH_RCV_MSG;
  event.ext[0] = reinterpret_cast<uint64_t>(&wakeup_buffer_);
  event.ext[1] = sizeof(wakeup_buffer_);

  kr = kevent64(kqueue_, &event, 1, nullptr, 0, 0, nullptr);
  CHECK_EQ(kr, KERN_SUCCESS);
}

TaskLoopForIOMac::~TaskLoopForIOMac() {
  CHECK(async_socket_readers_.empty());
}

void TaskLoopForIOMac::Run() {
  while (true) {
    // The last task that ran may have introduced a new |SocketReader| that
    // increased our |event_count_|, and thus the number of event filters that
    // we care about notifications for. In order for the kqueue to notify us of
    // multiple unique events (that are keyed on (ident, filter) pairs, see
    // https://www.manpagez.com/man/2/kevent64/) in one go, we expand our
    // |events_| vector, which is where kevent64 pushes potentially multiple
    // events that is has received. We of course could choose to not expand the
    // size of this vector, and then we'd only be able to service a single event
    // and source after every kevent64 call.
    events_.resize(event_count_);

    timespec timeout{0, 0};
    int rv = kevent64(kqueue_, nullptr, 0, events_.data(), events_.size(), 0, quit_when_idle_ ? &timeout : nullptr);

    // At this point we had to have read at least one event from the kernel.
    CHECK_GE(rv, 1);

    // If |quit_| is set, that is a no-brainer, we have to just quit. But if |quit_when_idle_| is set, things are not as obvious:
    //   - If the event type we're processing is EVFILT_READ, then 
    if (quit_ || (quit_when_idle_ && queue_.empty()))
      break;

    ProcessQueuedEvents(rv);

  } // while (true).

  // We need to reset |quit_| when |Run()| actually completes, so that we can
  // call |Run()| again later.
  quit_ = false;
  quit_when_idle_ = false;
}

void TaskLoopForIOMac::ProcessQueuedEvents(int num_events) {
  CHECK_GE(num_events, 1);

  for (int i = 0; i < num_events; ++i) {
    auto* event = &events_[i];

    if (event->filter == EVFILT_READ) {
      int fd = event->ident;

      mutex_.lock();
      auto* socket_reader = async_socket_readers_[fd];
      mutex_.unlock();

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
}

void TaskLoopForIOMac::WatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();
  std::vector<kevent64_s> events;

  kevent64_s new_event{};
  new_event.ident = fd;
  new_event.flags = EV_ADD;
  new_event.filter = EVFILT_READ;
  events.push_back(new_event);

  // Invoke kevent64 not to listen to events, but to supply a changelist of
  // event filters that we're interested in being notified about from the
  // kernel.
  int rv = kevent64(kqueue_, events.data(), events.size(), nullptr, 0, 0,
                    nullptr);
  CHECK_GE(rv, 0);

  mutex_.lock();
  // A socket reader can only be registered once.
  CHECK_EQ(async_socket_readers_.find(fd), async_socket_readers_.end());
  async_socket_readers_[fd] = socket_reader;
  event_count_++;
  mutex_.unlock();
}

void TaskLoopForIOMac::UnwatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();
  std::vector<kevent64_s> events;

  kevent64_s new_event{};
  new_event.ident = fd;
  new_event.flags = EV_DELETE;
  new_event.filter = EVFILT_READ;
  events.push_back(new_event);

  // Invoke kevent64 not to listen to events, but to supply a changelist of
  // event filters that we're interested in being notified about from the
  // kernel.
  int rv = kevent64(kqueue_, events.data(), events.size(), nullptr, 0, 0,
                    nullptr);
  CHECK_GE(rv, 0);

  mutex_.lock();
  CHECK(!async_socket_readers_.empty());
  async_socket_readers_.erase(fd);
  event_count_--;
  mutex_.unlock();
}

void TaskLoopForIOMac::PostTask(Callback cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  mutex_.unlock();
  MachWakeup();
}

void TaskLoopForIOMac::Quit() {
  mutex_.lock();
  quit_ = true;
  mutex_.unlock();
  MachWakeup();
}

void TaskLoopForIOMac::QuitWhenIdle() {
  mutex_.lock();
  quit_when_idle_ = true;
  mutex_.unlock();
  MachWakeup();
}

void TaskLoopForIOMac::MachWakeup() {
  // Send an empty message to |wakeup_|. There are three things that can happen
  // here:
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
}

}; // namespace base
