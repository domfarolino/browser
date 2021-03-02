#ifndef BASE_TEST_HELPER_H_
#define BASE_TEST_HELPER_H_

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/simple_thread.h"
#include "gtest/gtest.h"

namespace base {

int get_random_wait();
void producer(std::queue<std::string>& q, base::Mutex& mutex,
              base::ConditionVariable& condition);
void consumer(std::queue<std::string>& q, base::Mutex& mutex,
              base::ConditionVariable& condition);

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

class BaseThreading : public testing::Test {
 public:
  void PostTasksBackToMainThread(
      std::shared_ptr<base::TaskRunner> main_thread_task_runner,
      std::shared_ptr<base::TaskLoop> task_loop);
  void MainThreadTask();
  void TaskOne();
  void TaskTwo(int input);
  void TaskThree(TaskParam);
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

}  // namespace base

#endif  // BASE_TEST_BASE_THREADING_HELPER_H_
