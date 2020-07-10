#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <pthread.h>
#include <unistd.h>

#include <chrono>
#include <memory>

#include "base/check.h"

namespace base {

// This class is not directly usable until Thread::Delegate is implemented. Use
// SimpleThread for now.

class Thread {
public:
  // This is what the thread uses to actually run.
  class Delegate {
  public:
    virtual ~Delegate() {}
    virtual void Run() = 0;
    virtual void Quit() = 0;
  };

  Thread();
  void Start();
  void Quit() { delegate_->Quit(); }

  // This method is run for the duration of the physical thread's lifetime. When
  // it exits, the thread is terminated.
  void ThreadMain();

  // C++ thread similar methods.
  static void sleep_for(std::chrono::milliseconds ms);
  void join();

protected:
  // TODO(domfarolino): Remove the template param pack here.
  template <typename... Ts>
  static void* ThreadFunc(void* in);

  std::unique_ptr<Delegate> delegate_;

private:
  pthread_t id_;
  pthread_attr_t attributes_;
};

} // namespace base

#endif // BASE_THREADING_THREAD_H_
