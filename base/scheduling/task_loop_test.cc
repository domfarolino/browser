#include "gtest/gtest.h"

#include "base/scheduling/task_loop.h"
#include "base/threading/simple_thread.h"

// These tests are general TaskLoop tests that should pass for all kinds of task
// loops regardless of |base::ThreadType|.

namespace base {

TEST(TaskLoopTest, QuitBeforeRun) {
  auto task_loop = TaskLoop::Create(ThreadType::WORKER);
  task_loop->Quit();
  task_loop->Run(); // Loop should immediately quit. Test should not time out.
}

TEST(TaskLoopTest, RunQuitRunQuit) {
  auto task_loop = TaskLoop::Create(ThreadType::WORKER);

  SimpleThread t1([](Callback quit_closure) {
    quit_closure();
  }, task_loop->QuitClosure());
  task_loop->Run();

  SimpleThread t2([](Callback quit_closure) {
    quit_closure();
  }, task_loop->QuitClosure());
  task_loop->Run();
}

}; // namespace base
