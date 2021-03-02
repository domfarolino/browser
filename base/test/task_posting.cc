#include <chrono>
#include <iostream>

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/thread.h"
#include "base/test/helper.h"
#include "gtest/gtest.h"

namespace base {

TEST_F(BaseThreading, TaskPosting) {
  base::Thread worker_thread;
  worker_thread.Start();
  //PostTasks(worker_thread);

  auto task_runner = worker_thread.GetTaskRunner();
  task_runner->PostTask(std::bind(&BaseThreading::TaskOne, this));
  task_runner->PostTask(std::bind(&BaseThreading::TaskTwo, this, 2));
  TaskParam param(42);
  task_runner->PostTask(std::bind(&BaseThreading::TaskThree, this, param));

  base::Thread::sleep_for(std::chrono::milliseconds(1000));

  worker_thread.Quit();
  worker_thread.join();

  ASSERT_EQ(task1_complete, true);
  ASSERT_EQ(task2_complete, true);
  ASSERT_EQ(task3_complete, true);
}

}  // namespace base
