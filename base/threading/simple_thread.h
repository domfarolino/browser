#ifndef BASE_THREADING_SIMPLE_THREAD_H_
#define BASE_THREADING_SIMPLE_THREAD_H_

#include <memory>

#include "base/threading/thread.h"

namespace base {

class SimpleThread : public Thread {
public:
  template <typename... Ts>
  class SimpleThreadDelegate : public Thread::Delegate {
  public:
    template <typename F>
    SimpleThreadDelegate(F&& func, Ts&&... args)
        : f_(std::forward<F>(func)),
          args_(std::make_tuple(std::forward<Ts>(args)...)) {}

    void Run() {
      helper::invoker(f_, args_);
    }

  private:
    std::function<void (Ts...)> f_;
    std::tuple<Ts...> args_;
  };

  template <typename F, typename... Ts>
  SimpleThread(F&& func, Ts&&... args) : Thread() {
    delegate_.reset(
      new SimpleThreadDelegate<Ts...>(std::forward<F>(func),
                                      std::forward<Ts>(args)...));

    // Start the thread with the overridden delegate.
    Start();
  }
};

} // namespace base

#endif // BASE_THREADING_SIMPLE_THREAD_H_
