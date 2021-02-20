#include <pthread.h>
#include <unistd.h>

#include "base/threading/thread.h"

#include "base/check.h"
#include "base/scheduling/task_loop.h"

namespace base {

Thread::Thread(ThreadType type) : type_(type) {
  pthread_attr_init(&attributes_);
}

void Thread::Start() {
  // Given subclasses a chance to override their own |delegate_|.
  if (!delegate_) {
    delegate_.reset();
    delegate_ = TaskLoop::Create(type_);
  }

  CHECK(delegate_);
  pthread_create(&id_, &attributes_, ThreadFunc, this);
}

TaskRunner* Thread::GetTaskRunner() {
  return delegate_->GetTaskRunner();
}

void Thread::Quit() {
  delegate_->Quit();
}

// This method is run for the duration of the physical thread's lifetime. When
// it exits, the thread is terminated.
void Thread::ThreadMain() {
  CHECK(delegate_);
  delegate_->Run();
}

// static
void Thread::sleep_for(std::chrono::milliseconds ms) {
  usleep(ms.count() * 1000);
}

void Thread::join() {
  pthread_join(id_, nullptr);
}

// static
void* Thread::ThreadFunc(void* in) {
  Thread* thread = (Thread*)(in);
  CHECK(thread);

  thread->ThreadMain();
  pthread_exit(0);
}

} // namespace base
