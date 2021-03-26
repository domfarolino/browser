#include "base/scheduling/thread_task_runner.h"

#include <memory>

#include "base/check.h"
#include "base/scheduling/task_runner.h"

namespace base {

static std::weak_ptr<TaskLoop> g_ui_task_loop;
static std::weak_ptr<TaskLoop> g_io_task_loop;
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
