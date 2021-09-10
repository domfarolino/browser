#ifndef BASE_THREADING_SIMPLE_THREAD_H_
#define BASE_THREADING_SIMPLE_THREAD_H_

#include <memory>
#include <tuple>
#include <utility>

#include "base/callback.h"
#include "base/threading/thread.h"

namespace base {

class TaskRunner;

class SimpleThread : public Thread {
public:
  class SimpleThreadDelegate : public Thread::Delegate {
   public:
    explicit SimpleThreadDelegate(OnceClosure f) : f_(std::move(f)) {}

    // Thread::Delegate implementation.
    void BindToCurrentThread(ThreadType) override {}
    void Run() override {
      f_();
    }
    std::shared_ptr<TaskRunner> GetTaskRunner() override { NOTREACHED(); }
    void Quit() override {}
    void QuitWhenIdle() override {}

   private:
    OnceClosure f_;
  };

  template <typename F, typename... Ts>
  SimpleThread(F func, Ts... args) : Thread() {
    OnceClosure f = BindOnce(std::forward<F>(func), std::forward<Ts>(args)...);
    delegate_.reset(new SimpleThreadDelegate(std::move(f)));

    // Start the thread with the overridden delegate.
    Start();
  }
};

} // namespace base

#endif // BASE_THREADING_SIMPLE_THREAD_H_
