#ifndef BASE_SCHEDULING_THREAD_TASK_RUNNER_H_
#define BASE_SCHEDULING_THREAD_TASK_RUNNER_H_

#include <memory>

#include "base/check.h"
#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"

namespace base {

void SetThreadTaskRunner(std::shared_ptr<TaskRunner>);
void SetUIThreadTaskLoop(std::weak_ptr<TaskLoop>);
void SetIOThreadTaskLoop(std::weak_ptr<TaskLoop>);

std::shared_ptr<TaskLoop> GetUIThreadTaskLoop();
std::shared_ptr<TaskLoop> GetIOThreadTaskLoop();
std::shared_ptr<TaskRunner> GetThreadTaskRunner();

}; // namespace base

#endif // BASE_SCHEDULING_THREAD_TASK_RUNNER_H_
