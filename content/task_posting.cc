#include <iostream>

#include "base/threading/thread.h"

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
}
void TaskTwo(int input) {
  std::cout << std::endl << "I'm TaskTwo, and I'm being invoked with intetger: " << input << std::endl;
}
void TaskThree(TaskParam param) {
  std::cout << std::endl << "I'm TaskThree, and I'm being invoked with object whose data is: " << param.get_data() << std::endl;
}

// This runs on the main thread.
void PostTasks(base::Thread& thread) {
  int task_number;
  while (1) {
    std::cout << "Enter 1, 2, or 3 to post tasks 1, 2, or 3 respectively. Or 0 to quit: ";
    std::cin >> task_number;

    if (!task_number)
      break;

    if (task_number == 1) {
      thread.PostTask(std::bind(&TaskOne));
    } else if (task_number == 2) {
      int input;
      std::cout << "Enter TaskTwo's input: ";
      std::cin >> input;
      thread.PostTask(std::bind(&TaskTwo, input));
    } else {
      int input;
      std::cout << "Enter TaskThree's input: ";
      std::cin >> input;
      TaskParam param(input);
      auto task  = std::bind(&TaskThree, param);
      thread.PostTask(std::move(task));
    }
  }

  thread.Quit();
}

int main() {
  base::Thread worker_thread;
  worker_thread.Start();
  PostTasks(worker_thread);
  worker_thread.join();
}
