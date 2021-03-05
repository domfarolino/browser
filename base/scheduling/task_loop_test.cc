#include "gtest/gtest.h"

#include <iostream>

#include "base/scheduling/task_loop.h"
#include "base/threading/simple_thread.h"

TEST(TaskLoopTest, QuitBeforeRun) {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::WORKER);
  task_loop->Quit();
  task_loop->Run(); // Loop should immediately quit. Test should not time out.
}

TEST(TaskLoopTest, RunQuitRunQuit) {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::WORKER);

  base::SimpleThread t1([](base::Callback quit_closure) {
    quit_closure();
  }, task_loop->QuitClosure());
  task_loop->Run();

  base::SimpleThread t2([](base::Callback quit_closure) {
    quit_closure();
  }, task_loop->QuitClosure());
  task_loop->Run();
}
