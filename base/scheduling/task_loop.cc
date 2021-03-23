#include "base/scheduling/task_loop.h"

#include <memory>

#include "base/build_config.h"
#include "base/check.h"
#if defined(OS_MACOS)
#include "base/scheduling/task_loop_for_io.h"
#endif
#include "base/scheduling/task_loop_for_worker.h"
#include "base/scheduling/thread_task_runner.h"

namespace base {

void TaskLoop::BindToCurrentThread() {
  printf("BindToCurrentThread() being called\n");
  SetThreadTaskRunner(GetTaskRunner());
  CHECK(GetThreadTaskRunner());
}

// static
std::shared_ptr<TaskLoop> TaskLoop::Create(ThreadType type) {
  switch (type) {
    case ThreadType::WORKER:
      return std::shared_ptr<TaskLoopForWorker>(new TaskLoopForWorker());
    case ThreadType::UI:
      NOTREACHED();
      return std::shared_ptr<TaskLoopForWorker>();
    case ThreadType::IO:
#if defined(OS_MACOS)
      return std::shared_ptr<TaskLoopForIO>(new TaskLoopForIO());
#else
      NOTREACHED();
      return std::shared_ptr<TaskLoopForWorker>();
#endif
  }

  NOTREACHED();
  return std::shared_ptr<TaskLoopForWorker>();
}

std::shared_ptr<TaskRunner> TaskLoop::GetTaskRunner() {
  return std::shared_ptr<TaskRunner>(new TaskRunner(GetWeakPtr()));
}

Callback TaskLoop::QuitClosure() {
  return std::bind(&TaskLoop::Quit, this);
}

}; // namespace base
