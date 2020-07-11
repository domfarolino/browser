#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <pthread.h>
#include <unistd.h>

#include <chrono>
#include <memory>

#include "base/check.h"
#include "base/helper.h"

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
  };

  Thread() {
    pthread_attr_init(&attributes_);
  }

  void Start() {
    // TODO(domfarolino): If |delegate_| is null here (i.e., hasn't been
    // overridden by a subclass), create a new default Thread::Delegate.
    pthread_create(&id_, &attributes_, ThreadFunc, this);
  }

  // This method is run for the duration of the physical thread's lifetime. When
  // it exits, the thread is terminated.
  void ThreadMain() {
    CHECK(delegate_);
    delegate_->Run();
  }

  static void sleep_for(std::chrono::milliseconds ms) {
    usleep(ms.count() * 1000);
  }

  void join() {
    pthread_join(id_, nullptr);
  }

protected:
  template <typename... Ts>
  static void* ThreadFunc(void* in) {
    Thread* thread = (Thread*)(in);
    CHECK(thread);

    thread->ThreadMain();
    pthread_exit(0);
  }

  std::unique_ptr<Delegate> delegate_;

private:
  pthread_t id_;
  pthread_attr_t attributes_;
};

} // namespace base

#endif // BASE_THREADING_THREAD_H_
