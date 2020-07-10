#ifndef BASE_SCHEDULING_TASK_LOOP_H_
#define BASE_SCHEDULING_TASK_LOOP_H_

#include <queue>

#include "base/threading/thread.h"

namespace base {

class TaskLoop : public Thread::Delegate {
public:
  using Callback = std::function<void()>;
  TaskLoop() {}
  ~TaskLoop() {}

  TaskLoop(TaskLoop&) = delete;
  TaskLoop(TaskLoop&&) = delete;
  TaskLoop& operator=(const TaskLoop&) = delete;

  // Can be called from any thread.
  void PostTask(Callback cb) {
    mutex_.lock();

    q_.push(std::move(cb));

    mutex_.unlock();
    cv_.notify_one();
  }

  void Run() override {
    while (1) {
      cv_.wait(mutex_, [&]() -> bool{
        bool can_skip_waiting = (q_.empty() == false || quit_);
        return can_skip_waiting;
      });

      if (quit_)
        break;

      // We now own the lock for |q_|.
      CHECK(q_.size());
      Callback cb = std::move(q_.front());
      q_.pop();
      cv_.release_lock();

      ExecuteTask(std::move(cb));
    }
  }

  // Should this be thread-safe?
  void Quit() override {
    quit_ = true;
    cv_.notify_one();
  }

private:
  // TODO(domfarolino): Should this arg be a rvalue reference?
  void ExecuteTask(Callback cb) { NOTREACHED(); }

  // The mutex and condition variable are used to lock |q_|, and notify TaskLoop
  // when a task is ready to be executed.
  base::Mutex mutex_;
  base::ConditionVariable cv_;
  std::queue<Callback> q_;

  // Set to |true| only once in a TaskLoop's lifetime.
  bool quit_ = false;
};

} // namespace base

#endif //BASE_SCHEDULING_TASK_LOOP_H_
