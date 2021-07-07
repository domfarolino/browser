#include "base/threading/thread.h"

#include <pthread.h>
#include <unistd.h>

#include <chrono>
#include <memory>

#include "base/check.h"
#include "base/scheduling/task_loop.h"

namespace base {

Thread::Thread(ThreadType type) : type_(type) {
  pthread_attr_init(&attributes_);
}

Thread::~Thread() {
  Stop();
}

void Thread::Start() {
  CHECK(!started_);
  started_ = true;

  // Give subclasses a chance to override their own |delegate_|.
  if (!delegate_) {
    delegate_.reset();
    delegate_ = TaskLoop::CreateUnbound(type_);
  }

  CHECK(delegate_);
  pthread_create(&id_, &attributes_, ThreadFunc, this);
}

std::shared_ptr<TaskRunner> Thread::GetTaskRunner() {
  return delegate_->GetTaskRunner();
}

void Thread::Stop() {
  // We can't CHECK(started_) here because Stop() should be idempotent.
  started_ = false;

  // |delegate_| might be null if |Start()| has never been called.
  if (delegate_)
    delegate_->Quit();

  join();
}

// This method is run for the duration of the physical thread's lifetime. When
// it exits, the thread is terminated.
void Thread::ThreadMain() {
  CHECK(delegate_);
  delegate_->BindToCurrentThread(type_);
  delegate_->Run();
}

// static
void Thread::sleep_for(std::chrono::milliseconds ms) {
  usleep(ms.count() * 1000);
}

void Thread::join() {
  pthread_join(id_, nullptr);

  // TODO(domfarolino): Document this.
  started_ = false;
}

// static
void* Thread::ThreadFunc(void* in) {
  Thread* thread = (Thread*)(in);
  CHECK(thread);

  thread->ThreadMain();
  pthread_exit(0);
}

} // namespace base
