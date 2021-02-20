#ifndef BASE_SCHEDULING_TASK_RUNNER_H_
#define BASE_SCHEDULING_TASK_RUNNER_H_

#include "base/helper.h"

namespace base {

class TaskRunner {
 public:
  virtual void PostTask(Callback cb) = 0;
};

}; // namespace base

#endif // BASE_SCHEDULING_TASK_RUNNER_H_
