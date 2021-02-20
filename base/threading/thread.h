#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <pthread.h>
#include <unistd.h>

#include <chrono>
#include <memory>

#include "base/check.h"
#include "base/helper.h"

namespace base {

class TaskRunner;

// The |ThreadType| of a given thread defines what its underlying task loop is
// capable of.
enum class ThreadType {
  // This type of thread *only* supports executing manually posted tasks, e.g.,
  // via |ThreadDelegate::PostTask()|.
  WORKER,

  // This type of thread supports waiting for and serializing native UI events
  // from windowing libraries, in addition to manually posted tasks.
  UI,

  // This type of thread supports asynchronous IO in addition to manually posted
  // tasks. Asynchronous IO consists of message pipes specific to the underlying
  // platform, as well as file descriptors. This thread type should be used by
  // IPC implementations.
  IO,
};

class Thread {
 public:
  Thread(ThreadType type = ThreadType::WORKER);

  // This is what the thread uses to actually run.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void Run() = 0;
    virtual TaskRunner* GetTaskRunner() = 0;
    virtual void Quit() = 0;
  };

  void Start();
  TaskRunner* GetTaskRunner();
  void Quit();

  // This method is run for the duration of the physical thread's lifetime. When
  // it exits, the thread is terminated.
  void ThreadMain();

  // C++ thread similar methods.
  static void sleep_for(std::chrono::milliseconds ms);
  void join();

 protected:
  static void* ThreadFunc(void* in);

  std::unique_ptr<Delegate> delegate_;

 private:
  ThreadType type_;
  pthread_t id_;
  pthread_attr_t attributes_;
};

} // namespace base

#endif // BASE_THREADING_THREAD_H_
