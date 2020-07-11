#ifndef BASE_SYNCHRONIZATION_H_
#define BASE_SYNCHRONIZATION_H_

#include <pthread.h>

#include <condition_variable>
#include <mutex> // std::unique_lock.

#include "check.h"

namespace base {

enum class ThreadMode {
  kUsingCpp,
  kUsingPthread,
};

class Mutex {
public:
  Mutex(): Mutex(ThreadMode::kUsingCpp) {}
  Mutex(ThreadMode mode): mode_(mode) {
    CHECK(!pthread_mutex_init(&pthread_mutex_, nullptr));
  }

  Mutex(Mutex&) = delete;
  Mutex(Mutex&&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  operator std::mutex&() {
    CHECK_EQ(mode_, ThreadMode::kUsingCpp);
    return cpp_mutex_;
  }
  pthread_mutex_t& GetAsPthreadMutex() {
    CHECK_EQ(mode_, ThreadMode::kUsingPthread);
    return pthread_mutex_;
  }

  void lock() {
    if (mode_ == ThreadMode::kUsingCpp) {
      cpp_mutex_.lock();
    } else {
      pthread_mutex_lock(&pthread_mutex_);
    }
  }

  void unlock() {
    if (mode_ == ThreadMode::kUsingCpp) {
      cpp_mutex_.unlock();
    } else {
      pthread_mutex_unlock(&pthread_mutex_);
    }
  }

  bool is_locked() {
    if (mode_ == ThreadMode::kUsingCpp) {
      if (cpp_mutex_.try_lock()) {
        unlock();
        return false;
      }

      return true;
    } else {
      if (pthread_mutex_trylock(&pthread_mutex_) == 0) {
        unlock();
        return false;
      }

      return true;
    }
  }

  ~Mutex() {
    pthread_mutex_destroy(&pthread_mutex_);
  }

private:
  ThreadMode mode_;
  std::mutex cpp_mutex_;
  pthread_mutex_t pthread_mutex_;
};

// It might be good to consider making a Lock class, that takes a lock over a
// Mutex. That way the behavior and ownership is simpler, and more consistent
// between the ThreadModes. The current way is fine for now though, just a
// little implicit.

class ConditionVariable {
public:
  ConditionVariable(): ConditionVariable(ThreadMode::kUsingCpp) {}
  ConditionVariable(ThreadMode mode): mode_(mode) {
    CHECK(!pthread_cond_init(&pthread_condition_, nullptr));
  }
  ConditionVariable (ConditionVariable&) = delete;
  ConditionVariable (ConditionVariable&&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;


  void wait(Mutex& mutex, std::function<bool()> predicate) {
    if (mode_ == ThreadMode::kUsingCpp) {
      lock_ = std::unique_lock<std::mutex>(mutex);
      cpp_condition_.wait(lock_, predicate);
    } else {
      mutex.lock();
      mutex_ = &mutex;
      while (!predicate()) {
        pthread_cond_wait(&pthread_condition_, &mutex.GetAsPthreadMutex());
      }
    }
  }

  void release_lock() {
    if (mode_ == ThreadMode::kUsingCpp) {
      CHECK(lock_);
      // In kUsingCpp mode, we use a std::unique_lock.
      lock_.unlock();
    } else {
      CHECK(mutex_);
      CHECK(mutex_->is_locked());
      // Just defers to the pthread functions.
      mutex_->unlock();
    }
  }

  void notify_one() {
    if (mode_ == ThreadMode::kUsingCpp) {
      cpp_condition_.notify_one();
    } else {
      pthread_cond_signal(&pthread_condition_);
    }
  }

  ~ConditionVariable() {
    pthread_cond_destroy(&pthread_condition_);
  }

private:
  ThreadMode mode_;
  std::condition_variable cpp_condition_;
  // A ConditionVariable takes a lock over a Mutex when |wait()| is called.
  std::unique_lock<std::mutex> lock_;

  base::Mutex* mutex_ = nullptr;
  pthread_cond_t pthread_condition_;
};

} // namespace base

#endif // BASE_SYNCHRONIZATION_H_
