#include "base/scheduling/thread_task_runner.h"

#include <memory>

#include "base/check.h"
#include "base/scheduling/task_runner.h"

namespace base {

static thread_local std::shared_ptr<TaskRunner> g_thread_task_runner;

void SetThreadTaskRunner(std::shared_ptr<TaskRunner> task_runner) {
  g_thread_task_runner = std::move(task_runner);
}

std::shared_ptr<TaskRunner> GetThreadTaskRunner() {
  CHECK(g_thread_task_runner);
  return g_thread_task_runner;
}

}; // namespace base
