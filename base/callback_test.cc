#include "base/callback.h"

#include <memory>

#include "gtest/gtest.h"

namespace base {

void AcceptCopyable(int n) {}
void IncrementReference(int& n) { n++; }
void AcceptMoveOnly(std::unique_ptr<int>) {}

TEST(Closure, Copyable) {
  int n = 1;
  Closure closure = BindOnce(AcceptCopyable, n);
}

TEST(Closure, IncrementReference) {
  int n = 1;
  Closure closure = BindOnce(IncrementReference, n);
  closure();
  EXPECT_EQ(n, 2);
}

TEST(Closure, MoveOnly) {
  Closure closure = BindOnce(AcceptMoveOnly, std::make_unique<int>(1));
}

//////////////////////

TEST(Lambda, LambdaBindVariable) {
  bool executed = false;
  std::function<void()> closure = std::bind([](bool& executed){
    executed = true;
  }, std::ref(executed));

  closure();
  EXPECT_TRUE(executed);
}
TEST(Closure, LambdaBindVariable) {
  bool executed = false;
  Closure closure = BindOnce([](bool& executed){
    executed = true;
    // This documents a behavior difference from std::bind, note that we don't
    // have to use std::ref(executed) below, because BindOnce() does not copy
    // arguments that are to be passed by reference to the given lambda.
  }, executed);

  closure();
  EXPECT_TRUE(executed);
}

TEST(Lambda, LambdaReferenceCapture) {
  bool executed = false;
  std::function<void()> closure = [&](){
    executed = true;
  };

  closure();
  EXPECT_TRUE(executed);
}
TEST(Closure, LambdaReferenceCapture) {
  bool executed = false;
  Closure closure = [&](){
    executed = true;
  };

  closure();
  EXPECT_TRUE(executed);
}
TEST(Closure, LambdaReferenceCapture_WithBind) {
  bool executed = false;
  Closure closure = BindOnce([&](){
    executed = true;
  });

  closure();
  EXPECT_TRUE(executed);
}

TEST(Lambda, LambdaCaptureVariableReference) {
  bool executed = false;
  std::function<void()> closure = [&executed](){
    executed = true;
  };

  closure();
  EXPECT_TRUE(executed);
}
TEST(Closure, LambdaCaptureVariableReference) {
  bool executed = false;
  Closure closure = [&executed](){
    executed = true;
  };

  closure();
  EXPECT_TRUE(executed);
}
TEST(Closure, LambdaCaptureVariableReference_WithBind) {
  bool executed = false;
  Closure closure = BindOnce([&executed](){
    executed = true;
  });

  closure();
  EXPECT_TRUE(executed);
}

//////////////////////

TEST(Closure, MovedClosureCannotBeCalled) {
  bool executed = false;
  Closure closure = [&executed](){
    executed = true;
  };

  Closure destination_closure = std::move(closure);
  EXPECT_FALSE(executed);
  destination_closure();
  EXPECT_TRUE(executed);

  ASSERT_DEATH({ closure(); }, "bind_state_");
}

class CopyableAndMovable {
 public:
  CopyableAndMovable() = default;
  CopyableAndMovable(const CopyableAndMovable& other) {
    CopyableAndMovable::copy_ctor_called++;
  }
  CopyableAndMovable& operator=(const CopyableAndMovable& other) {
    CopyableAndMovable::copy_ctor_called++;
    return *this;
  }

  CopyableAndMovable(CopyableAndMovable&& other) {
    CopyableAndMovable::move_ctor_called++;
  }
  CopyableAndMovable& operator=(CopyableAndMovable&& other) {
    CopyableAndMovable::move_ctor_called++;
    return *this;
  }

  static int copy_ctor_called;
  static int move_ctor_called;
};

int CopyableAndMovable::copy_ctor_called = 0;
int CopyableAndMovable::move_ctor_called = 0;

void AcceptCopyableAndMovableByValue(CopyableAndMovable obj) {}
void AcceptCopyableAndMovableByReference(CopyableAndMovable& obj) {}

TEST(Closure, AcceptCopyableAndMovableByValue_PassedByValue) {
  CopyableAndMovable obj;
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  Closure closure = BindOnce(AcceptCopyableAndMovableByValue, obj);

  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 1);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 2);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 3);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  // Reset counters:
  CopyableAndMovable::copy_ctor_called = 0;
  CopyableAndMovable::move_ctor_called = 0;
}

TEST(Closure, AcceptCopyableAndMovableByValue_PassedByReference) {
  CopyableAndMovable obj;
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  Closure closure = BindOnce(AcceptCopyableAndMovableByValue, std::ref(obj));

  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 1);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 2);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 3);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  // Reset counters:
  CopyableAndMovable::copy_ctor_called = 0;
  CopyableAndMovable::move_ctor_called = 0;
}

// TODO(domfarolino): This should fail to compile.
TEST(Closure, AcceptCopyableAndMovableByReference_PassedByValue) {
  CopyableAndMovable obj;
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  Closure closure = BindOnce(AcceptCopyableAndMovableByReference, obj);

  // Reset counters:
  CopyableAndMovable::copy_ctor_called = 0;
  CopyableAndMovable::move_ctor_called = 0;
}

TEST(Closure, AcceptCopyableAndMovableByReference_PassedByReference) {
  CopyableAndMovable obj;
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  Closure closure = BindOnce(AcceptCopyableAndMovableByReference, std::ref(obj));

  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 1);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 2);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  // Reset counters:
  CopyableAndMovable::copy_ctor_called = 0;
  CopyableAndMovable::move_ctor_called = 0;
}

}; // namespace base
