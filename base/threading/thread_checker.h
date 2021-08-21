#ifndef BASE_THREADING_THREAD_CHECKER_H_
#define BASE_THREADING_THREAD_CHECKER_H_

#include <memory>

namespace base {

class TaskLoop;

class ThreadChecker {
 public:
  ThreadChecker();

  bool CalledOnConstructedThread();

 private:
  std::weak_ptr<TaskLoop> current_task_loop_;
};

} // namespace base

#endif // BASE_THREADING_THREAD_CHECKER_H_
