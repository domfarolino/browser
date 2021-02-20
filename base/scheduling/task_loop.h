#ifndef BASE_SCHEDULING_TASK_LOOP_H_
#define BASE_SCHEDULING_TASK_LOOP_H_

#include <memory>
#include <queue>

#include "base/helper.h"
#include "base/scheduling/task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"
#include "base/threading/thread.h"

namespace base {

// |TaskLoop| should be uniquely owned by the owner of |Thread::Delegate|
// (concretely, that's |Thread|). However, have to inherit from
// |std::enable_shared_from_this| so that we can hand std::weak_ptrs pointing to
// |this| out to |TaskRunner|. For more information see the documentation above
// |TaskRunner|.
class TaskLoop : public Thread::Delegate,
                 public TaskRunner::Delegate,
                 public std::enable_shared_from_this<TaskLoop> {
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
  std::shared_ptr<TaskRunner> GetTaskRunner() override {
    return std::shared_ptr<TaskRunner>(new TaskRunner(GetWeakPtr()));
  }

  // TaskRunner::Delegate implementation.
  // Can be called from any thread.
  // This is used to expose only |TaskLoop::PostTask()| method to |TaskRunner|s
  // that post tasks to this loop.
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

 private:
  std::weak_ptr<TaskLoop> GetWeakPtr() {
    return shared_from_this();
  }
};

} // namespace base

#endif // BASE_SCHEDULING_TASK_LOOP_H_
