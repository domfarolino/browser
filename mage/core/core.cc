#include "mage/core/core.h"

#include <unistd.h>

#include <cstdlib>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/endpoint.h"
#include "mage/core/util.h"

namespace mage {

Core* g_core;
base::TaskLoopForIO* g_task_loop;

void Core::Init(base::TaskLoopForIO* task_loop) {
  srand(getpid());

  g_core = new Core();
  g_task_loop = task_loop;
}

Core* Core::Get() {
  return g_core;
}

base::TaskLoopForIO* Core::GetTaskLoop() {
  return g_task_loop;
}

MageHandle Core::GetNextMageHandle() {
  return next_available_handle_++;
}

}; // namspace mage
