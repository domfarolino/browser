#include <chrono>
#include <iostream>

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "gtest/gtest.h"

namespace base {

class TaskParam {
 public:
  TaskParam(int data): data_(data) {}
  TaskParam(const TaskParam& other) {
    data_ = other.data_;
    std::cout << "Copy constructor" << std::endl;
  }
  TaskParam(const TaskParam&& other) {
    data_ = other.data_;
    std::cout << "Move constructor" << std::endl;
  }

  int get_data() {
    return data_;
  }

 private:
  int data_;
};

class TaskLoopWorkerTest : public testing::Test {
 public:
  // This runs on a worker thread, and posts tasks back to the main thread.
  void PostTasksBackToMainThread(
     std::shared_ptr<base::TaskRunner> main_thread_task_runner,
     std::shared_ptr<base::TaskLoop> task_loop) {
    for (int i = 0; i < 5; ++i) {
      base::Thread::sleep_for(std::chrono::milliseconds(200));
      main_thread_task_runner->PostTask(std::bind(&TaskLoopWorkerTest::MainThreadTask, this));
    }
    task_loop->Quit();
  }

  void MainThreadTask() {
    std::cout << std::endl << "I'm a task that was posted to the main thread from a base::SimpleThread" << std::endl;
    main_thread_complete = true;
  }

  // These run off-main-thread.
  void TaskOne() {
    std::cout << std::endl << "I'm TaskOne, and I'm being invoked" << std::endl;
    task1_complete = true;
  }

  void TaskTwo(int input) {
    std::cout << std::endl << "I'm TaskTwo, and I'm being invoked with intetger: " << input << std::endl;
    task2_complete = true;
  }

  void TaskThree(TaskParam param) {
    std::cout << std::endl << "I'm TaskThree, and I'm being invoked with object whose data is: " << param.get_data() << std::endl;
    task3_complete = true;
  }

  bool main_thread_complete;
  bool task1_complete;
  bool task2_complete;
  bool task3_complete;

 protected:
  void SetUp() override {
    main_thread_complete = false;
    task1_complete = false;
    task2_complete = false;
    task3_complete = false;
  }

  // virtual void TearDown() {
  // }
};

TEST_F(TaskLoopWorkerTest, BindTaskLoopToCurrentThread) {
  auto task_loop = base::TaskLoop::Create(base::ThreadType::WORKER);
  base::SimpleThread simple(std::bind(&TaskLoopWorkerTest::PostTasksBackToMainThread,
                                      this,
                                      task_loop->GetTaskRunner(),
                                      task_loop));

  task_loop->Run();
  simple.join();

  ASSERT_EQ(main_thread_complete, true);
}

TEST_F(TaskLoopWorkerTest, TaskPosting) {
  base::Thread worker_thread;
  worker_thread.Start();
  //PostTasks(worker_thread);

  auto task_runner = worker_thread.GetTaskRunner();
  task_runner->PostTask(std::bind(&TaskLoopWorkerTest::TaskOne, this));
  task_runner->PostTask(std::bind(&TaskLoopWorkerTest::TaskTwo, this, 2));
  TaskParam param(42);
  task_runner->PostTask(std::bind(&TaskLoopWorkerTest::TaskThree, this, param));

  base::Thread::sleep_for(std::chrono::milliseconds(1000));

  worker_thread.Quit();
  worker_thread.join();

  ASSERT_EQ(task1_complete, true);
  ASSERT_EQ(task2_complete, true);
  ASSERT_EQ(task3_complete, true);
}

};  // namespace base
