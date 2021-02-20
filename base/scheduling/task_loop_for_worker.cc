#include "base/scheduling/task_loop_for_worker.h"

namespace base {

void TaskLoopForWorker::Run() {
  while (1) {
    cv_.wait(mutex_, [&]() -> bool{
      bool can_skip_waiting = (queue_.empty() == false || quit_);
      return can_skip_waiting;
    });

    if (quit_)
      break;

    // We now own the lock for |queue_|.
    CHECK(queue_.size());
    Callback cb = std::move(queue_.front());
    queue_.pop();
    cv_.release_lock();

    ExecuteTask(std::move(cb));
  }
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

}; // namespace base
