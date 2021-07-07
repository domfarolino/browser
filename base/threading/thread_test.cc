#include <memory>

#include "base/build_config.h"
#include "base/scheduling/scheduling_handles.h"
#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/thread.h"
#include "gtest/gtest.h"

namespace base {

class ThreadTest : public testing::Test,
                   public testing::WithParamInterface<ThreadType> {
 public:
  ThreadTest() = default;

  // Provides meaningful param names instead of /0 and /1.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case ThreadType::UI:
        return "UI";
      case ThreadType::IO:
        return "IO";
      case ThreadType::WORKER:
        return "WORKER";
    }
  }

  virtual void SetUp() override {
    CHECK(!thread);
    thread = std::unique_ptr<Thread>(new Thread(GetParam()));
  }

  virtual void TearDown() override {
    thread.reset();
  }

 protected:
  std::unique_ptr<Thread> thread;
};

// These two tests are regression tests for
// https://github.com/domfarolino/browser/issues/40.
TEST_P(ThreadTest, ImplicitDestroyRaceConditionOnThreadDelegate) {
  thread->Start();
}
TEST_P(ThreadTest, ImplicitDestroyRaceConditionOnThreadDelegateConditionVariable) {
  thread->Start();
  thread->GetTaskRunner()->PostTask([](){});
}

// Death test.
// The documentation [1] strongly recommends using the "*DeathTest" suffix for
// naming, however I can not get this to work with parameterized tests for some
// reason. Everything appears to be fine without the suffix though.
// [1]: https://github.com/google/googletest/blob/master/docs/advanced.md#death-test-naming
TEST_P(ThreadTest, StartStart) {
  thread->Start();
  ASSERT_DEATH({ thread->Start(); }, "!started_");
}

TEST_P(ThreadTest, StartStopStop) {
  thread->Start();
  thread->Stop();
  thread->Stop();
}

TEST_P(ThreadTest, StopStopWithoutStart) {
  thread->Stop();
  thread->Stop();
}

TEST_P(ThreadTest, StartStopStartStop) {
  thread->Start();
  thread->Stop();
  thread->Start();
  thread->Stop();
}

TEST_P(ThreadTest, StartJoinStartJoin) {
  thread->Start();

  bool first_task_ran = false;
  thread->GetTaskRunner()->PostTask([&](){
    first_task_ran = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  });
  thread->join();
  EXPECT_TRUE(first_task_ran);

  thread->Start();

  bool second_task_ran = false;
  thread->GetTaskRunner()->PostTask([&](){
    second_task_ran = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  });
  thread->join();
  EXPECT_TRUE(second_task_ran);
}

// This test ensures that |base::Thread::Stop()| is idempotent; that is, if you
// call it multiple times and then post a real task, you don't have to call
// |base::Thread::Start()| as many times just to finally get to the next task.
// The |Stop()|s are not queued.
TEST_P(ThreadTest, StopIsIdempotent) {
  thread->Start();

  thread->Stop();
  thread->Stop();
  thread->Stop();
  thread->Stop();
  thread->Stop();
  thread->Stop();

  bool task_ran = false;
  thread->GetTaskRunner()->PostTask([&](){
    task_ran = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  });

  // Start the thread. Its underlying TaskLoop should immediately quit because
  // it has had its Quit() method invoked.
  thread->Start();
  thread->join();
  EXPECT_FALSE(task_ran);

  // Start the thread again. Its underlying TaskLoop should run the queued task
  // now that its Quit() method has not been invoked since the last time. We
  // observe the effects of the task running by checking the |task_ran|
  // variable.
  thread->Start();
  thread->join();
  EXPECT_TRUE(task_ran);
}

#if defined(OS_MACOS)
INSTANTIATE_TEST_SUITE_P(ThreadTest,
                         ThreadTest,
                         testing::Values(ThreadType::UI, ThreadType::IO, ThreadType::WORKER),
                         &ThreadTest::DescribeParams);
#else
// ThreadType::IO is only supported on macos for now.
INSTANTIATE_TEST_SUITE_P(ThreadTest,
                         ThreadTest,
                         testing::Values(ThreadType::UI, ThreadType::WORKER),
                         &ThreadTest::DescribeParams);
#endif

}; // namespace base
