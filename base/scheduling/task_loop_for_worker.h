#ifndef BASE_SCHEDULING_TASK_LOOP_FOR_WORKER_H_
#define BASE_SCHEDULING_TASK_LOOP_FOR_WORKER_H_

#include <queue>

#include "base/helper.h"
#include "base/scheduling/task_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"
#include "base/threading/thread.h"

namespace base {

class TaskLoopForWorker : public TaskLoop {
public:
  TaskLoopForWorker() {}
  ~TaskLoopForWorker() {}

  TaskLoopForWorker(TaskLoopForWorker&) = delete;
  TaskLoopForWorker(TaskLoopForWorker&&) = delete;
  TaskLoopForWorker& operator=(const TaskLoopForWorker&) = delete;

  // Thread::Delegate implementation.
  void Run() override;
  // Can be called from any thread.
  void PostTask(Callback cb) override;
  // Can be called from any thread.
  void Quit() override;

private:
  // The mutex and condition variable are used to lock |queue_|, and notify the
  // task loop when a task has been posted and is ready to execute.
  base::Mutex mutex_;
  base::ConditionVariable cv_;
  std::queue<Callback> queue_;
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_FOR_WORKER_H_
