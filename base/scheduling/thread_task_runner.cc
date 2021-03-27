#include "base/scheduling/thread_task_runner.h"

#include <memory>

#include "base/check.h"

namespace base {

// These two are weak because these pointers may outlive the underlying
// |TaskLoop| (especially in tests) and we don't want to keep it alive here.
static std::weak_ptr<TaskLoop> g_ui_task_loop;
static std::weak_ptr<TaskLoop> g_io_task_loop;
// This is a strong pointer because it doesn't matter how long this |TaskRunner|
// is alive with respect to the underlying |TaskLoop| it posts to, since that is
// the point of |TaskRunner|s.
static thread_local std::shared_ptr<TaskRunner> g_thread_task_runner;

void SetUIThreadTaskLoop(std::weak_ptr<TaskLoop> ui_task_loop) {
  g_ui_task_loop = ui_task_loop;
}

void SetIOThreadTaskLoop(std::weak_ptr<TaskLoop> io_task_loop) {
  g_io_task_loop = io_task_loop;
}

void SetThreadTaskRunner(std::shared_ptr<TaskRunner> task_runner) {
  g_thread_task_runner = std::move(task_runner);
}

std::shared_ptr<TaskLoop> GetUIThreadTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_ui_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskLoop> GetIOThreadTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_io_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskRunner> GetThreadTaskRunner() {
  CHECK(g_thread_task_runner);
  return g_thread_task_runner;
}

}; // namespace base
