#include <chrono>
#include <iostream>

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/thread.h"
#include "gtest/gtest.h"

static bool task1_complete = false;
static bool task2_complete = false;
static bool task3_complete = false;

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

TEST(BaseThreading, TaskPosting) {
  base::Thread worker_thread;
  worker_thread.Start();
  //PostTasks(worker_thread);

  auto task_runner = worker_thread.GetTaskRunner();
  task_runner->PostTask(std::bind(&TaskOne));
  task_runner->PostTask(std::bind(&TaskTwo, 2));
  TaskParam param(42);
  task_runner->PostTask(std::bind(&TaskThree, param));

  base::Thread::sleep_for(std::chrono::milliseconds(1000));

  worker_thread.Quit();
  worker_thread.join();

  ASSERT_EQ(task1_complete, true);
  ASSERT_EQ(task2_complete, true);
  ASSERT_EQ(task3_complete, true);
}
