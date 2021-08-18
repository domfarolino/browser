#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_IO_MAC_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_IO_MAC_H_

#include <mach/mach.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/event.h>

#include <map>
#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/scheduling/task_loop.h"
#include "base/synchronization/mutex.h"

namespace base {

// Note that this |TaskLoop| variant only works on macOS, as it uses Mach ports
// and |kevent64()| to block on OS-level IPC primitives. Once we support
// detecting which platform we're on, we'll need to make an equivalent class
// that uses glib, libevent, or something for Linux, and ensure that we
// instantiate the correct one depending on the platform we're running on.
class TaskLoopForIOMac : public TaskLoop {
 public:
  // |SocketReader|s register themselves with a |TaskLoopForIO| associated with
  // a given file descriptor. They are notified asynchronously when the file
  // descriptor can be read from.
  class SocketReader {
   public:
    SocketReader(int fd): fd_(fd) {}
    virtual ~SocketReader() = default;
    int Socket() { return fd_; }
    virtual void OnCanReadFromSocket() = 0;
   protected:
    int fd_;
  };

  TaskLoopForIOMac();
  ~TaskLoopForIOMac();

  TaskLoopForIOMac(TaskLoopForIOMac&) = delete;
  TaskLoopForIOMac(TaskLoopForIOMac&&) = delete;
  TaskLoopForIOMac& operator=(const TaskLoopForIOMac&) = delete;

  // Thread::Delegate implementation.
  void Run() override;
  // Can be called from any thread.
  void Quit() override;

  // TaskRunner::Delegate implementation.
  // Can be called from any thread.
  void PostTask(Callback cb) override;

  void QuitWhenIdle() override;

  // Can be called from any thread.
  void WatchSocket(SocketReader* reader);
  void UnwatchSocket(SocketReader* reader);

  // Can be called from any thread (it is *implicitly* thread-safe).
  void MachWakeup();

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

  // The number of event types (filters) that we're interested in listening to
  // via |kqueue_|. There is always at least 1, for the |wakeup_| port (or
  // |port_set_|), but increase this count whenever we register a new e.g.,
  // |SocketReader|.
  size_t event_count_ = 1;
  // This buffer is where events from the kernel queue are stored after calls to
  // |kevent64|. This buffer is consulted in |Run()|, where events are pulled
  // and the loop responds to them.
  std::vector<kevent64_s> events_{event_count_};
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_FOR_IO_MAC_H_
