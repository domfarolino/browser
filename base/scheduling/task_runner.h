#ifndef BASE_SCHEDULING_TASK_RUNNER_H_
#define BASE_SCHEDULING_TASK_RUNNER_H_

#include <memory> // std::weak_ptr.

#include "base/helper.h"

namespace base {

class TaskRunner final {
 public:

  // |TaskRunner::PostTask()| basically is just a pass-through to
  // |TaskRunner::Delegate::PostTask()|. The reason for this is because we want
  // to support the case where |this| and other |TaskRunner|s are out alive in
  // the wild, but the underlying |TaskLoop| that we post tasks to (via
  // |weak_delegate_|) has actually been destroyed. Therefore before |this|
  // posts a task to |weak_delegate_|, we check to see if it is still around by
  // checking its std::weak_ptr. If it is not around we silently fail to post
  // the task. The reason for this whole dance is so that users of |TaskRunner|
  // don't have to perform this check manually everytime they want to post a
  // task. It is only expected that |weak_delegate_| is not around during
  // shutdown, which might not be clear to consumers of |TaskRunner|, which is
  // why we abstract the check out to this class. In some cases however, it
  // might be useful for |TaskRunner| consumers to know when their task has
  // failed to post. In that case, we can consider making
  // |TaskRunner::PostTask()| return a boolean, false when a task failed to
  // post, that way the particularly nervous consumers can check this if they
  // want.
  class Delegate {
   public:
    virtual void PostTask(Callback cb) = 0;
  };

  TaskRunner(std::weak_ptr<Delegate> weak_delegate) : weak_delegate_(weak_delegate) {}

  void PostTask(Callback cb) {
    if (auto delegate_ = weak_delegate_.lock())
      delegate_->PostTask(std::move(cb));

    // Maybe we should do some logging when |weak_delegate_| is destroyed, just
    // to verify that this doesn't happen in unexpected cases i.e., not during
    // thread shutdown.
  }

 private:
  std::weak_ptr<Delegate> weak_delegate_;
};

}; // namespace base

#endif // BASE_SCHEDULING_TASK_RUNNER_H_
