#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_IO_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_IO_H_

#include <mach/mach.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/event.h>

#include <map>
#include <queue>
#include <vector>

#include "base/helper.h"
#include "base/scheduling/task_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"
#include "base/threading/thread.h"

namespace base {

// Note that this |TaskLoop| variant only works on macOS, as it uses Mach ports
// and |kevent64()| to block on OS-level IPC primitives. Once we support
// detecting which platform we're on, we'll need to make an equivalent class
// that uses glib, libevent, or something for Linux, and ensure that we
// instantiate the correct one depending on the platform we're running on.
class TaskLoopForIO : public TaskLoop {
 public:
  // |SocketReader|s register themselves with a |TaskLoopForIO| associated with
  // a given file descriptor. They are notified asynchronously when the file
  // descriptor can be read from.
  class SocketReader {
   public:
    virtual ~SocketReader() = default;
    SocketReader(int fd): fd_(fd) {}
    virtual void OnCanReadFromSocket() = 0;
   protected:
    int fd_;
  };

  TaskLoopForIO() : kqueue_(kqueue()) {
    kern_return_t kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &wakeup_);
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
  ~TaskLoopForIO() = default;

  TaskLoopForIO(TaskLoopForIO&) = delete;
  TaskLoopForIO(TaskLoopForIO&&) = delete;
  TaskLoopForIO& operator=(const TaskLoopForIO&) = delete;

  // Thread::Delegate implementation.
  void Run() override;
  // Can be called from any thread.
  void Quit() override;

  // TODO(domfarolino): Once we have platform-specific implementations of
  // |TaskLoopForIO|, I think we'll want to make this pure virtual, and have
  // impls provide a proper implementation.
  void WatchSocket(int fd, SocketReader* reader);

  // TaskRunner::Delegate implementation.
  // Can be called from any thread.
  void PostTask(Callback cb) override;
 private:

  // The kqueue that drives the task loop.
  int kqueue_;

  // Receive right to which an empty Mach message is sent to wake up the pump
  // in response to |PostTask()|.
  mach_port_t wakeup_;
  // Scratch buffer that is used to receive the message sent to |wakeup_|.
  mach_msg_empty_rcv_t wakeup_buffer_;

  // These are listeners that get notified when their file descriptor has been
  // written to and is ready to read from. |SocketReader|s are expected to
  // unregister themselves from this map upon destruction.
  std::map<int, SocketReader*> async_socket_readers_;

  // The number of events scheduled on the |kqueue_|. There is always at least
  // 1, for the |wakeup_| port (or |port_set_|).
  size_t event_count_ = 1;
  // Buffer used by DoInternalWork() to be notified of triggered events. This
  // is always at least |event_count_|-sized.
  std::vector<kevent64_s> events_{event_count_};
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_FOR_IO_H_
