#include "helper.h"

#include "base/scheduling/task_loop.h"
#include "base/scheduling/task_runner.h"
#include "base/threading/simple_thread.h"

namespace base {


void BaseThreading::MainThreadTask() {
  std::cout << std::endl << "I'm a task that was posted to the main thread from a base::SimpleThread" << std::endl;
  main_thread_complete = true;
}

// This runs on a worker thread, and posts tasks back to the main thread.
void BaseThreading::PostTasksBackToMainThread(
    std::shared_ptr<base::TaskRunner> main_thread_task_runner,
    std::shared_ptr<base::TaskLoop> task_loop) {
  for (int i = 0; i < 5; ++i) {
    base::Thread::sleep_for(std::chrono::milliseconds(200));
    main_thread_task_runner->PostTask(std::bind(&BaseThreading::MainThreadTask, this));
  }
  task_loop->Quit();
}

// These run off-main-thread.
void BaseThreading::TaskOne() {
  std::cout << std::endl << "I'm TaskOne, and I'm being invoked" << std::endl;
  task1_complete = true;
}

void BaseThreading::TaskTwo(int input) {
  std::cout << std::endl << "I'm TaskTwo, and I'm being invoked with intetger: " << input << std::endl;
  task2_complete = true;
}

void BaseThreading::TaskThree(TaskParam param) {
  std::cout << std::endl << "I'm TaskThree, and I'm being invoked with object whose data is: " << param.get_data() << std::endl;
  task3_complete = true;
}

// Returns a random wait period in ms, weighted to return lower milliseconds
// more frequently.
int get_random_wait() {
  int random = rand() % 10 + 1; // [1, 10]
  if (random <= 4) random = 0; // [1, 4]
  else if (random <= 6) random = 20; // [5, 6]
  else if (random <= 8) random = 70; // [7, 8]
  else if (random <= 9) random = 100; // [9, 9]
  else if (random == 10) random = 200; // [10, 10]
  return random;
}

void producer(std::queue<std::string>& q, base::Mutex& mutex,
              base::ConditionVariable& condition) {
  // Guaranteed to produce at least one string.
  int num_strings_to_produce = rand() % 100 + 1, i = 0;

  std::string tmp;
  while (num_strings_to_produce) {
    mutex.lock();

    // If this is our last string, make it the "quit" string.
    if (num_strings_to_produce == 1) {
      q.push("quit");
      std::cout << "\x1B[33m Producer producing quit message\x1b[00m" << std::endl;
    } else {
      tmp = "string" + std::to_string(i++);
      q.push(tmp);
      std::cout << "\x1B[33m Producer producing '" << tmp << "'\x1b[00m" << std::endl;
    }
    mutex.unlock();
    condition.notify_one();

    // Delay the producer's message by random time.
    base::Thread::sleep_for(std::chrono::milliseconds(get_random_wait()));
    num_strings_to_produce--;
  }
}

void consumer(std::queue<std::string>& q, base::Mutex& mutex,
              base::ConditionVariable& condition) {
  std::string data = "";
  while (data != "quit") {
    // Optionally wait, if the predicate function returns false.
    condition.wait(mutex, [&]() -> bool{
                            bool can_skip_waiting = (q.empty() == false);
                            if (can_skip_waiting == false)
                              std::cout << "\x1B[34m   Consumer waiting for more input\x1B[00m" << std::endl;
                            return can_skip_waiting;
                          });

    EXPECT_EQ(mutex.is_locked(), true);

    data = q.front();
    std::cout << "\x1B[32m Consumer consuming '" << data << "'\x1B[00m" << std::endl;
    q.pop();
    condition.release_lock();
  }
}

}  // namespace base
