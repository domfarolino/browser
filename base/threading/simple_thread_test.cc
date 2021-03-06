#include "base/callback.h"
#include "base/threading/simple_thread.h"
#include "gtest/gtest.h"

namespace base {

void OffMainThread(bool& thread_ran) {
  thread_ran = true;
}

void OffMainThreadRunCallback(Callback callback) {
  callback();
}

TEST(SimpleThreadTest, BasicPassByReference) {
  bool thread_ran = false;
  SimpleThread thread(OffMainThread, std::ref(thread_ran));
  thread.join();
  EXPECT_EQ(thread_ran, true);
}

TEST(SimpleThreadTest, BasicPassByValue) {
  bool thread_ran = false;
  SimpleThread thread(OffMainThread, thread_ran);
  thread.join();
  EXPECT_EQ(thread_ran, false);
}

TEST(SimpleThreadTest, BasicWithLambdaReferenceCapture) {
  bool thread_ran = false;
  SimpleThread thread([&](){
    thread_ran = true;
  });
  thread.join();
  EXPECT_EQ(thread_ran, true);
}

TEST(SimpleThreadTest, BasicWithLambdaCallback) {
  bool thread_ran = false;
  SimpleThread thread(OffMainThreadRunCallback, [&](){
    thread_ran = true;
  });
  thread.join();
  EXPECT_EQ(thread_ran, true);
}

TEST(SimpleThreadTest, BasicWithLambdaAndLambdaParameterPassed) {
  bool thread_ran = false;
  SimpleThread thread([](Callback callback){
    callback();
  }, [&](){
    thread_ran = true;
  });
  thread.join();
  EXPECT_EQ(thread_ran, true);
}

}; // namespace base