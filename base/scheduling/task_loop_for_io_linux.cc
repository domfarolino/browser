#include "base/scheduling/task_loop_for_io_linux.h"

#include <event.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/errno.h>
#include <sys/eventfd.h>
#include <sys/socket.h>

#include <iostream>
#include <vector>

namespace base {

TaskLoopForIOLinux::TaskLoopForIOLinux() {
  epollfd_ = epoll_create1(0);
  CHECK(epollfd_ != -1);

  eventfd_ = eventfd(0, 0);
  CHECK(eventfd_ != -1);
  epoll_data_t event_data;
  event_data.fd = eventfd_;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data = event_data;

  int kr = epoll_ctl(epollfd_, EPOLL_CTL_ADD, eventfd_, &ev);
  CHECK(kr != -1);
}

TaskLoopForIOLinux::~TaskLoopForIOLinux() {
  CHECK(async_socket_readers_.empty());
}

void TaskLoopForIOLinux::Run() {
  while (true) {
    struct epoll_event events[MAX_EVENTS];
    int timeout = quit_when_idle_ ? 0 : -1;
    int rv = epoll_wait(epollfd_, events, MAX_EVENTS, timeout);

    // We grab a lock so that neither of the following are tampered with on
    // another thread while we are reading/writing them:
    //   - |queue_|
    //   - |async_socket_readers_|
    mutex_.lock();

    // At this point we have at least one event from the kernel, unless we're in
    // the |quit_when_idle_| mode and have nothing to do. We detect this
    // idleness via `rv == 0` later and break.
    CHECK(rv >= 1 || (quit_when_idle_ && rv == 0));

    // If |quit_| is set, that is a no-brainer, we have to just quit. But if
    // |quit_when_idle_| is set, we can only *actually* quit if we're idle. We
    // can detect if we're idle by verifying that `rv == 0`. If
    // `quit_when_idle_ && rv >= 1`, then we are not idle. This can happen in a
    // number of ways:
    //   - `quit_when_idle_ && rv == 1 && queue_.empty()`:
    //     This can happen if the loop is idling in the `kevent64()` call above,
    //     and then |QuitWhenIdle()| is called. This wakes up the loop with a
    //     MACHPORT event, but puts nothing on the queue. This is handled in
    //     |ProcessQueuedEvents()|. If no reads or tasks are queued by the time
    //     the next loop iteration comes around, `kevent64()` will not block, rv
    //     will be 0, and we will detect that we're idle and break.
    //   - `quit_when_idle_ && rv == 1 && !queue_.empty()`:
    //     This can happen after we've already processed the "empty" MACHPORT
    //     event from |QuitWhileIdle()| and nothing was on the queue, but then
    //     before the next loop iteration, a real task was added to the queue
    //     and woke us up. The subsequent call to `kevent64()` would reflect
    //     this with `rv == 1` and the queue having a task pushed to it.
    if (quit_ || (quit_when_idle_ && rv == 0)) {
      mutex_.unlock();
      break;
    }

    CHECK_GE(rv, 1);

    // Process any queued events (the number of which is `rv`).
    for (int i = 0; i < rv; ++i) {
      struct epoll_event* event = &events[i];
      int fd = event->data.fd;
      if (fd == eventfd_) {
        // If the queue is empty but we have an |eventfd| event to process, this
        // must just be a |QuitWhenIdle()| waking us up with no actual work to do.
        // We'll do nothing.
        if (queue_.empty()) {
          CHECK(quit_when_idle_);
          mutex_.unlock();
          continue;
        }

        CHECK(queue_.size());
        OnceClosure cb = std::move(queue_.front());
        queue_.pop();
        mutex_.unlock();

        ExecuteTask(std::move(cb));
      } else {
        auto* socket_reader = async_socket_readers_[fd];
        mutex_.unlock();
        CHECK(socket_reader);
        socket_reader->OnCanReadFromSocket();
      }
    }

    // By this point, |mutex_| will always be unlocked so that we can lock it
    // for the next iteration. Note that we can't just unlock it here at the end
    // of this loop, to make things simple. We have to unlock it before we
    // process whatever event type we're processing or else we are prone to
    // deadlocks. For example, if we keep |mutex_| locked while we run a task
    // that we pull from the |queue_|, then if that task calls PostTask() on
    // this loop, then it will try and lock |mutex_| and deadlock forever.
  } // while (true).

  // We need to reset |quit_| when |Run()| actually completes, so that we can
  // call |Run()| again later.
  quit_ = false;
  quit_when_idle_ = false;
}

void TaskLoopForIOLinux::WatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();

  epoll_data_t event_data;
  event_data.fd = fd;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data = event_data;

  int kr = epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev);
  CHECK(kr != -1);

  mutex_.lock();
  // A socket reader can only be registered once.
  CHECK_EQ(async_socket_readers_.find(fd), async_socket_readers_.end());
  async_socket_readers_[fd] = socket_reader;
  event_count_++;
  mutex_.unlock();
}

void TaskLoopForIOLinux::UnwatchSocket(SocketReader* socket_reader) {
  CHECK(socket_reader);
  int fd = socket_reader->Socket();

  epoll_data_t event_data;
  event_data.fd = fd;
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data = event_data;

  int kr = epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
  CHECK(kr != -1);

  mutex_.lock();
  CHECK(!async_socket_readers_.empty());
  async_socket_readers_.erase(fd);
  event_count_--;
  mutex_.unlock();
}

void TaskLoopForIOLinux::PostTask(OnceClosure cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  mutex_.unlock();
  Wakeup();
}

void TaskLoopForIOLinux::Quit() {
  mutex_.lock();
  quit_ = true;
  mutex_.unlock();
  Wakeup();
}

void TaskLoopForIOLinux::QuitWhenIdle() {
  mutex_.lock();
  quit_when_idle_ = true;
  mutex_.unlock();
  Wakeup();
}

void TaskLoopForIOLinux::Wakeup() {
  // Send an empty message to |eventfd_|. There are three things that can happen
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
  int msg = 0;
  ssize_t kr = write(eventfd_, &msg, sizeof(uint64_t));
  CHECK(kr != 0);
}

}; // namespace base
