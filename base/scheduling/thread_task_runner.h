#ifndef BASE_SCHEDULING_THREAD_TASK_RUNNER_H_
#define BASE_SCHEDULING_THREAD_TASK_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/check.h"
#include "base/scheduling/task_runner.h"

namespace base {

void SetThreadTaskRunner(std::shared_ptr<TaskRunner>);
std::shared_ptr<TaskRunner> GetThreadTaskRunner();


}; // namespace base

#endif // BASE_SCHEDULING_THREAD_TASK_RUNNER_H_
