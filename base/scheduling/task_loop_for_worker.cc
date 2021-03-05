#include "base/scheduling/task_loop_for_worker.h"

namespace base {

void TaskLoopForWorker::Run() {
  while (1) {
    cv_.wait(mutex_, [&]() -> bool{
      bool can_skip_waiting = (queue_.empty() == false || quit_);
      return can_skip_waiting;
    });

    // We now own the lock for |queue_|, until we explicitly release it.
    if (quit_) {
      cv_.release_lock();
      break;
    }

    CHECK(queue_.size());
    Callback cb = std::move(queue_.front());
    queue_.pop();
    cv_.release_lock();

    ExecuteTask(std::move(cb));
  }

  // We need to reset |quit_| when |Run()| actually completes, so that we can
  // call |Run()| again later.
  quit_ = false;
}

void TaskLoopForWorker::PostTask(Callback cb) {
  mutex_.lock();
  queue_.push(std::move(cb));
  mutex_.unlock();

  cv_.notify_one();
}

void TaskLoopForWorker::Quit() {
  quit_ = true;
  cv_.notify_one();
}

Callback TaskLoopForWorker::QuitClosure() {
  return std::bind(&TaskLoopForWorker::Quit, this);
}

}; // namespace base
