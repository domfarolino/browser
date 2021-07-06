#include "base/threading/thread.h"
#include "base/scheduling/task_runner.h"
#include "gtest/gtest.h"

namespace base {

// These two tests are regression tests for
// https://github.com/domfarolino/browser/issues/40.
TEST(ThreadTest, ImplicitDestroyRaceConditionOnThreadDelegate) {
  base::Thread worker_thread;
  worker_thread.Start();
}
TEST(ThreadTest, ImplicitDestroyRaceConditionOnThreadDelegateConditionVariable) {
  base::Thread worker_thread;
  worker_thread.Start();
  worker_thread.GetTaskRunner()->PostTask([](){});
}

}; // namespace base
