#ifndef BASE_THREADING_SIMPLE_THREAD_H_
#define BASE_THREADING_SIMPLE_THREAD_H_

#include <memory>
#include <tuple>
#include <utility>

#include "base/helper.h"
#include "base/threading/thread.h"

namespace base {

class TaskRunner;

class SimpleThread : public Thread {
public:
  class SimpleThreadDelegate : public Thread::Delegate {
   public:
    explicit SimpleThreadDelegate(std::function<void()> f) : f_(f) {}

    // Thread::Delegate implementation.
    void Run() override {
      f_();
    }
    std::shared_ptr<TaskRunner> GetTaskRunner() override { NOTREACHED(); }
    void Quit() override {}

   private:
    std::function<void()> f_;
  };

  template <typename F, typename... Ts>
  SimpleThread(F func, Ts... args) : Thread() {
    std::function<void()> f{std::bind(func, args...)};
    delegate_.reset(new SimpleThreadDelegate(f));

    // Start the thread with the overridden delegate.
    Start();
  }
};

} // namespace base

#endif // BASE_THREADING_SIMPLE_THREAD_H_
