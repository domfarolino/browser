#include "base/scheduling/current_scheduling_handles.h"

#include <memory>

#include "base/check.h"

namespace base {

// Process-global pointers.
//
// These are weak process-global pointers to underlying |TaskLoop| that we wish
// to reference but not keep alive.
static std::weak_ptr<TaskLoop> g_ui_task_loop;
static std::weak_ptr<TaskLoop> g_io_task_loop;

// Thread-global pointers.
//
// These are pointers that are global only within a thread, so that the current
// thread can reference it's current |TaskLoop| if it exists, and a
// corresponding |TaskRunner|.
//
// The |TaskLoop| is a weak pointer for the reasons mentioned above. The
// |TaskRunner| pointer is strong because it doesn't matter how long it lives
// with respect to its underlying |TaskLoop| that it posts to, since that is
// precisely the point of |TaskRunner|s (see the documentation there).
static thread_local std::weak_ptr<TaskLoop> g_current_thread_task_loop;
static thread_local std::shared_ptr<TaskRunner> g_current_thread_task_runner;

void SetUIThreadTaskLoop(std::weak_ptr<TaskLoop> ui_task_loop) {
  g_ui_task_loop = ui_task_loop;
  // Also set the thread-local TaskLoop pointer.
  g_current_thread_task_loop = ui_task_loop;
}

void SetIOThreadTaskLoop(std::weak_ptr<TaskLoop> io_task_loop) {
  g_io_task_loop = io_task_loop;
  // Also set the thread-local TaskLoop pointer.
  g_current_thread_task_loop = io_task_loop;
}

void SetThreadTaskRunner(std::shared_ptr<TaskRunner> task_runner) {
  g_current_thread_task_runner = std::move(task_runner);
}

// The getters only return std::shared_ptrs so that their callers don't have to
// go through the dance of dereferencing it from a std::weak_ptr.
std::shared_ptr<TaskLoop> GetUIThreadTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_ui_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskLoop> GetIOThreadTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_io_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskLoop> GetCurrentTaskLoop() {
  std::shared_ptr<TaskLoop> task_loop = g_current_thread_task_loop.lock();
  return task_loop;
}

std::shared_ptr<TaskRunner> GetCurrentTaskRunner() {
  CHECK(g_current_thread_task_runner);
  return g_current_thread_task_runner;
}

}; // namespace base
