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

void TaskLoop::BindToCurrentThread(ThreadType type) {
  // Depending on |type|, create a process-global pointer to |this|.
  switch (type) {
    case ThreadType::UI:
      NOTREACHED();
      CHECK(!GetUIThreadTaskLoop());
      SetUIThreadTaskLoop(GetWeakPtr());
      CHECK(GetUIThreadTaskLoop());
      break;
    case ThreadType::IO:
      CHECK(!GetIOThreadTaskLoop());
      SetIOThreadTaskLoop(GetWeakPtr());
      CHECK(GetIOThreadTaskLoop());
      break;
    case ThreadType::WORKER:
      // We don't store process-global pointers to |ThreadType::WORKER|
      // TaskLoops because there can be more than one of these per process.
      break;
  }

  // Regardless of |type|, set the current thread's TaskRunner to
  // |this->GetTaskRunner()|.
  SetThreadTaskRunner(GetTaskRunner());
}

// static
std::shared_ptr<TaskLoop> TaskLoop::Create(ThreadType type) {
  std::shared_ptr<TaskLoop> task_loop;
  switch (type) {
    case ThreadType::WORKER:
      task_loop = std::shared_ptr<TaskLoopForWorker>(new TaskLoopForWorker());
      break;
    case ThreadType::UI:
      NOTREACHED();
      task_loop = std::shared_ptr<TaskLoopForWorker>();
      break;
    case ThreadType::IO:
#if defined(OS_MACOS)
      task_loop = std::shared_ptr<TaskLoopForIO>(new TaskLoopForIO());
      break;
#else
      NOTREACHED();
      task_loop = std::shared_ptr<TaskLoopForWorker>();
      break;
#endif
  }

  task_loop->BindToCurrentThread(type);
  return task_loop;
}

std::shared_ptr<TaskRunner> TaskLoop::GetTaskRunner() {
  return std::shared_ptr<TaskRunner>(new TaskRunner(GetWeakPtr()));
}

Callback TaskLoop::QuitClosure() {
  return std::bind(&TaskLoop::Quit, this);
}

}; // namespace base
