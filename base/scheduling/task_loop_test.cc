#include "gtest/gtest.h"

#include "base/build_config.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"
#include "base/threading/simple_thread.h"

// These tests are general TaskLoop tests that should pass for all kinds of task
// loops regardless of |base::ThreadType|.

namespace base {

class TaskLoopTest : public testing::Test,
                     public testing::WithParamInterface<ThreadType> {
 public:
  TaskLoopTest() : thread_type_(GetParam()) {}

  virtual void SetUp() override {
    task_loop = TaskLoop::Create(thread_type_);
  }

  virtual void TearDown() override {
    task_loop.reset();
  }

  std::shared_ptr<TaskLoop> task_loop;

 private:
  base::ThreadType thread_type_;
};

//////////      These tests only use |TaskLoop::Run()|      //////////
////////// RunUntilIdle() is explicitly tested further down //////////

TEST_P(TaskLoopTest, QuitBeforeRun) {
  task_loop->Quit();
  task_loop->Run(); // Loop should immediately quit. Test should not time out.
}

TEST_P(TaskLoopTest, PostTasksBeforeRun) {
  // When the loop is |Run()|, it immediately runs all posted tasks. We assert
  // that the first task is run by observing its side effects, and we assert
  // that the last task runs because the loop quits and the test does not time
  // out.
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });
  task_loop->PostTask(std::bind([](Callback quit_closure){
    quit_closure();
  }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(first_task_ran, true);
}

TEST_P(TaskLoopTest, RunQuitRunQuit) {
  bool first_task_ran = false;
  task_loop->PostTask(std::bind([&](Callback quit_closure){
    first_task_ran = true;
    quit_closure();
  }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(first_task_ran, true);

  bool second_task_ran = false;
  task_loop->PostTask(std::bind([&](Callback quit_closure){
    second_task_ran = true;
    quit_closure();
  }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, NestedTasks) {
  bool outer_task_ran = false;
  bool inner_task_ran = false;

  task_loop->PostTask([&](){
    outer_task_ran = true;
    task_loop->PostTask([&](){
      inner_task_ran = true;
      task_loop->Quit();
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->Run();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(inner_task_ran, true);
}

TEST_P(TaskLoopTest, GetCurrentThreadTaskRunner) {
  bool first_task_ran = false;
  base::GetCurrentThreadTaskRunner()->PostTask(
    std::bind([&](Callback quit_closure){
      first_task_ran = true;
      quit_closure();
    }, task_loop->QuitClosure()));

  task_loop->Run();
  EXPECT_EQ(first_task_ran, true);
}

TEST_P(TaskLoopTest, NestedGetCurrentThreadTaskRunner) {
  bool outer_task_ran = false;
  bool inner_task_ran = false;
  base::GetCurrentThreadTaskRunner()->PostTask([&](){
    outer_task_ran = true;
    base::GetCurrentThreadTaskRunner()->PostTask([&](){
      inner_task_ran = true;
      task_loop->Quit();
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->Run();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(inner_task_ran, true);
}

////////// These tests only use |TaskLoop::RunUntiIdle()| //////////

TEST_P(TaskLoopTest, QuitBeforeRunUntilIdle) {
  task_loop->Quit();
  // Loop should immediately quit. Test should not time out.
  task_loop->RunUntilIdle();
}

TEST_P(TaskLoopTest, PostTasksBeforeRunUntilIdle) {
  // When the loop is |RunUntilIdle()|, it immediately runs all posted tasks. We
  // assert that the first task is run by observing its side effects, and we
  // assert that |RunUntilIdle()| actually works by the test immediately ending
  // after the task is run and not timing out.
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });
  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleQuitRunUntilIdleQuit) {
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);

  bool second_task_ran = false;
  task_loop->PostTask([&](){
    second_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleMultipleTasks) {
  bool first_task_ran = false;
  task_loop->PostTask([&](){
    first_task_ran = true;
  });

  bool second_task_ran = false;
  task_loop->PostTask([&](){
    second_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleEarlyQuit) {
  bool first_task_ran = false;
  task_loop->PostTask(
    std::bind([&](Callback quit_closure){
      first_task_ran = true;
      quit_closure();
    }, task_loop->QuitClosure()));


  bool second_task_ran = false;
  task_loop->PostTask([&](){
    second_task_ran = true;
  });

  task_loop->RunUntilIdle();
  EXPECT_EQ(first_task_ran, true);
  EXPECT_EQ(second_task_ran, false);

  task_loop->RunUntilIdle();
  EXPECT_EQ(second_task_ran, true);
}

TEST_P(TaskLoopTest, RunUntilIdleDoesNotSnapshotTheEventQueueSize) {
  bool outer_task_ran = false;
  bool continuation_task_ran = false;

  task_loop->PostTask([&](){
    outer_task_ran = true;
    task_loop->PostTask([&](){
      continuation_task_ran = true;
    }); // Inner PostTask().
  }); // Outer PostTask().

  task_loop->RunUntilIdle();
  EXPECT_EQ(outer_task_ran, true);
  EXPECT_EQ(continuation_task_ran, true);
}

#if defined(OS_MACOS)
INSTANTIATE_TEST_SUITE_P(TaskLoopTest,
                         TaskLoopTest,
                         testing::Values(ThreadType::WORKER, ThreadType::IO));
#else
INSTANTIATE_TEST_SUITE_P(TaskLoopTest,
                         TaskLoopTest,
                         testing::Values(ThreadType::WORKER));
#endif

}; // namespace base
