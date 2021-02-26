#include <iostream>

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/simple_thread.h"
#include "gtest/gtest.h"

static bool main_thread_complete = false;

void MainThreadTask() {
  std::cout << std::endl << "I'm a task that was posted to the main thread from a base::SimpleThread" << std::endl;
  main_thread_complete = true;
}

// This runs on a worker thread, and posts tasks back to the main thread.
void PostTasksBackToMainThread(std::shared_ptr<base::TaskRunner> main_thread_task_runner, std::shared_ptr<base::TaskLoop> task_loop) {
    for (int i = 0; i < 5; ++i) {
      base::Thread::sleep_for(std::chrono::milliseconds(200));
      main_thread_task_runner->PostTask(std::bind(&MainThreadTask));
    }

    task_loop->Quit();
}

TEST(BaseThreading, BindTaskLoopToCurrentThread) {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::WORKER);
  base::SimpleThread simple(std::bind(&PostTasksBackToMainThread, task_loop->GetTaskRunner(), task_loop));

  task_loop->Run();
  simple.join();

  ASSERT_EQ(main_thread_complete, true);
}
