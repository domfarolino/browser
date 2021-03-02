#include <iostream>

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/test/helper.h"
#include "gtest/gtest.h"

namespace base {

TEST_F(BaseThreading, BindTaskLoopToCurrentThread) {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::WORKER);
  base::SimpleThread simple(std::bind(&BaseThreading::PostTasksBackToMainThread,
                                      this,
                                      task_loop->GetTaskRunner(),
                                      task_loop));

  task_loop->Run();
  simple.join();

  ASSERT_EQ(main_thread_complete, true);
}

}  // namespace base
