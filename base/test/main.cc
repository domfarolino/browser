#include <iostream>
#include <cstdlib> // rand()
#include <queue>

#include "base/threading/simple_thread.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/mutex.h"
#include "base/test/helper.h"
#include "gtest/gtest.h"

namespace base {

TEST_F(BaseThreading, ProducerConsumerQueue) {
  srand(time(NULL));
  base::Mutex mutex(base::ThreadMode::kUsingPthread);
  base::ConditionVariable condition(base::ThreadMode::kUsingPthread);

  std::queue<std::string> message_queue;
  base::SimpleThread producer_thread(producer, std::ref(message_queue), std::ref(mutex), std::ref(condition));
  consumer(message_queue, mutex, condition);
  producer_thread.join();
}

}  // namespace base
