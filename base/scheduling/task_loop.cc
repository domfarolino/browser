#include "base/check.h"
#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_loop_for_worker.h"

namespace base {

// static
std::unique_ptr<TaskLoop> TaskLoop::Create(ThreadType type) {
  switch (type) {
    case ThreadType::WORKER:
      printf("Creating a task loop of type worker\n");
      return std::unique_ptr<TaskLoopForWorker>(new TaskLoopForWorker());
    case ThreadType::UI:
    case ThreadType::IO:
      NOTREACHED();
      return std::unique_ptr<TaskLoopForWorker>();
  }
}

}; // namespace base
