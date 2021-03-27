#ifndef BASE_SCHEDULING_CURRENT_SCHEDULING_HANDLES_H_
#define BASE_SCHEDULING_CURRENT_SCHEDULING_HANDLES_H_

#include <memory>

namespace base {

class TaskLoop;
class TaskRunner;

void SetUIThreadTaskLoop(std::weak_ptr<TaskLoop>);
void SetIOThreadTaskLoop(std::weak_ptr<TaskLoop>);
void SetThreadTaskRunner(std::shared_ptr<TaskRunner>);

std::shared_ptr<TaskLoop> GetUIThreadTaskLoop();
std::shared_ptr<TaskLoop> GetIOThreadTaskLoop();
std::shared_ptr<TaskLoop> GetCurrentTaskLoop();
std::shared_ptr<TaskRunner> GetCurrentTaskRunner();

}; // namespace base

#endif // BASE_SCHEDULING_CURRENT_SCHEDULING_HANDLES_H_
