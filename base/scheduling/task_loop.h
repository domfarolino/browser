#ifndef BASE_SCHEDULING_TASK_LOOP_H_
#define BASE_SCHEDULING_TASK_LOOP_H_

#include <queue>

#include "base/helper.h"
#include "base/scheduling/task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"
#include "base/threading/thread.h"

namespace base {

class TaskLoop : public Thread::Delegate, public TaskRunner {
 public:
  TaskLoop() {}
  ~TaskLoop() {}

  TaskLoop(TaskLoop&) = delete;
  TaskLoop(TaskLoop&&) = delete;
  TaskLoop& operator=(const TaskLoop&) = delete;

  static std::unique_ptr<TaskLoop> Create(ThreadType type);

  // Thread::Delegate implementation.
  void Run() override = 0;
  // Can be called from any thread.
  void Quit() override = 0;
  // Can be called from any thread.
  // TODO(domfarolino): It is bad that this returns a raw pointer, this needs to
  // be changed.
  TaskRunner* GetTaskRunner() override {
    return this;
  }

  // TaskRunner implementation.
  // Can be called from any thread.
  // This is used to expose only the |PostTask()| method to consumers that one
  // to post tasks.
  void PostTask(Callback cb) override = 0;

 protected:
  // TODO(domfarolino): We may want to move this method down to the
  // |TaskLoopForWorker| base class instead of having it here for all |TaskLoop|
  // types.
  void ExecuteTask(Callback cb) {
    cb();
  }

  // Each concrete implementation of |TaskLoop| has its own members that control
  // when and on what the loop blocks, and how it is woken up, so this base
  // class has no members specific to the internals of the loop, besides a basic
  // |quit_| boolean that all loops share.

  // Set to |true| only once in the task loop's lifetime.
  bool quit_ = false;
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_H_
