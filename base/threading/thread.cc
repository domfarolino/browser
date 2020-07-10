#include <pthread.h>
#include <unistd.h>

#include "base/threading/thread.h"

#include "base/check.h"
#include "base/scheduling/task_loop.h"

namespace base {

Thread::Thread() {
  pthread_attr_init(&attributes_);
}

void Thread::Start() {
  if (!delegate_)
    delegate_.reset(new TaskLoop());
  CHECK(delegate_);
  pthread_create(&id_, &attributes_, ThreadFunc, this);
}

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

template <typename... Ts>
// static
void* Thread::ThreadFunc(void* in) {
  Thread* thread = (Thread*)(in);
  CHECK(thread);

  thread->ThreadMain();
  pthread_exit(0);
}

} // namespace base
