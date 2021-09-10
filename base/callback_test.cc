#include "base/callback.h"

#include <memory>

#include "gtest/gtest.h"

namespace base {

void AcceptNumberByValue(int n) { n++; }
void AcceptNumberByReference(int& n) { n++; }
void AcceptMoveOnly(std::unique_ptr<int>) {}

TEST(OnceClosure, InvokeTwice) {
  OnceClosure closure = BindOnce(AcceptNumberByValue, 1);
  closure();
  ASSERT_DEATH({ closure(); }, "bind_state_");
}

TEST(OnceClosure, Copyable) {
  int n = 1;
  OnceClosure closure = BindOnce(AcceptNumberByValue, n);
  closure();
  EXPECT_EQ(n, 1);
}

// This test has the same behavior and expectations as the one above, we're just
// testing that it *can* compile for completeness.
TEST(OnceClosure, Copyable_PassByStdRef) {
  int n = 1;
  OnceClosure closure = BindOnce(AcceptNumberByValue, std::ref(n));
  closure();
  EXPECT_EQ(n, 1);
}

// Note that the following do not compile in our implementation, nor do they
// compile in Chromium's implementation. They *do* compile with std::bind(), but
// this is is scary because `n` is treated like a value, not a reference without
// any warning! We require references to be passed explicitly by std::ref() in
// order to compile, whereas std::bind() requires std::ref() only in order to
// work properly. This is tested further down.
/*
TEST(OnceClosure, IncrementReferenceNoStdRef_NOCOMPILE) {
  int n = 1;
  OnceClosure closure = BindOnce(AcceptNumberByReference, n);
}
TEST(OnceClosure, IncrementReferenceNoStdRefPassByValue_NOCOMPILE) {
  OnceClosure closure = BindOnce(AcceptNumberByReference, 1);
}
*/

TEST(OnceClosure, IncrementReference) {
  int n = 1;
  OnceClosure closure = BindOnce(AcceptNumberByReference, std::ref(n));
  closure();
  EXPECT_EQ(n, 2);
}

TEST(OnceClosure, MoveOnly) {
  OnceClosure closure = BindOnce(AcceptMoveOnly, std::make_unique<int>(1));
  closure();
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
TEST(OnceClosure, LambdaBindVariable) {
  bool executed = false;
  OnceClosure closure = BindOnce([](bool& executed){
    executed = true;
    // This documents a behavior difference from std::bind, note that we don't
    // have to use std::ref(executed) below, because BindOnce() does not copy
    // arguments that are to be passed by reference to the given lambda.
  }, std::ref(executed));

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
TEST(OnceClosure, LambdaReferenceCapture) {
  bool executed = false;
  OnceClosure closure = [&](){
    executed = true;
  };

  closure();
  EXPECT_TRUE(executed);
}
TEST(OnceClosure, LambdaReferenceCapture_WithBind) {
  bool executed = false;
  OnceClosure closure = BindOnce([&](){
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
TEST(OnceClosure, LambdaCaptureVariableReference) {
  bool executed = false;
  OnceClosure closure = [&executed](){
    executed = true;
  };

  closure();
  EXPECT_TRUE(executed);
}
TEST(OnceClosure, LambdaCaptureVariableReference_WithBind) {
  bool executed = false;
  OnceClosure closure = BindOnce([&executed](){
    executed = true;
  });

  closure();
  EXPECT_TRUE(executed);
}

TEST(OnceClosure, MovedOnceClosureCannotBeCalled) {
  bool executed = false;
  OnceClosure closure = [&executed](){
    executed = true;
  };

  OnceClosure destination_closure = std::move(closure);
  EXPECT_FALSE(executed);
  destination_closure();
  EXPECT_TRUE(executed);

  ASSERT_DEATH({ closure(); }, "bind_state_");
}

class CopyOnly {
 public:
  CopyOnly() = default;
  CopyOnly(const CopyOnly& other) {
    CopyOnly::copy_ctor_called++;
  }
  CopyOnly& operator=(const CopyOnly& other) {
    CopyOnly::copy_ctor_called++;
    return *this;
  }

  static int copy_ctor_called;
};

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

class MoveOnly {
 public:
  MoveOnly() = default;
  MoveOnly(const MoveOnly& other) = delete;
  MoveOnly& operator=(const MoveOnly& other) = delete;
  MoveOnly(MoveOnly&&) {
    MoveOnly::move_ctor_called++;
  }
  MoveOnly& operator=(MoveOnly&& other) {
    MoveOnly::move_ctor_called++;
    return *this;
  }

  static int move_ctor_called;
};

int CopyOnly::copy_ctor_called = 0;

int CopyableAndMovable::copy_ctor_called = 0;
int CopyableAndMovable::move_ctor_called = 0;

int MoveOnly::move_ctor_called = 0;

void AcceptCopyOnlyByValue(CopyOnly) {}
void AcceptCopyOnlyByReference(CopyOnly&) {}

void AcceptCopyableAndMovableByValue(CopyableAndMovable) {}
void AcceptCopyableAndMovableByReference(CopyableAndMovable&) {}

void AcceptMoveOnlyByValue(MoveOnly) {}
void AcceptMoveOnlyByReference(MoveOnly&) {}

TEST(OnceClosure, AcceptCopyOnlyByValue_PassByValue) {
  CopyOnly obj;
  EXPECT_EQ(CopyOnly::copy_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptCopyOnlyByValue, obj);

  EXPECT_EQ(CopyOnly::copy_ctor_called, 1);
  closure();
  EXPECT_EQ(CopyOnly::copy_ctor_called, 2);

  // Reset counters:
  CopyOnly::copy_ctor_called = 0;
}

TEST(OnceClosure, AcceptCopyOnlyByValue_PassByStdRef) {
  CopyOnly obj;
  EXPECT_EQ(CopyOnly::copy_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptCopyOnlyByValue, std::ref(obj));

  EXPECT_EQ(CopyOnly::copy_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyOnly::copy_ctor_called, 1);

  // Reset counters:
  CopyOnly::copy_ctor_called = 0;
}

// Note that the `_PassByValue` variant of this will not compile.
TEST(OnceClosure, AcceptCopyOnlyByReference_PassByStdRef) {
  CopyOnly obj;
  EXPECT_EQ(CopyOnly::copy_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptCopyOnlyByReference, std::ref(obj));

  EXPECT_EQ(CopyOnly::copy_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyOnly::copy_ctor_called, 0);

  // Reset counters:
  CopyOnly::copy_ctor_called = 0;
}

TEST(OnceClosure, AcceptCopyableAndMovableByValue_PassByValue) {
  CopyableAndMovable obj;
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptCopyableAndMovableByValue, obj);

  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 1);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  // The reason the move constructor is called here *at all* is because
  // `OnceClosure` prefers moving arguments where possible, when invoking the
  // bound functor. This happens because it *moves* its argument tuple in order
  // to invoke the functor. What this means for arguments types that are
  // copy-only is that the argument is simply copied, instead of moved, from the
  // argument tuple. For types that support copying and moving, moving takes
  // place.
  //
  // Once we introduce something like `RepeatingOnceClosure`, the internal arg
  // tuple will never be moved, and only copies will happen (repeating closures
  // cannot accept move-only types at all).
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 1);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 1);

  // Reset counters:
  CopyableAndMovable::copy_ctor_called = 0;
  CopyableAndMovable::move_ctor_called = 0;
}

TEST(OnceClosure, AcceptCopyableAndMovableByValue_PassByStdRef) {
  CopyableAndMovable obj;
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptCopyableAndMovableByValue,
                                 std::ref(obj));

  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 1);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  // Reset counters:
  CopyableAndMovable::copy_ctor_called = 0;
  CopyableAndMovable::move_ctor_called = 0;
}

// Note that the `_PassByValue` variant of this will not compile.
TEST(OnceClosure, AcceptCopyableAndMovableByReference_PassByStdRef) {
  CopyableAndMovable obj;
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptCopyableAndMovableByReference,
                                 std::ref(obj));

  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);
  closure();
  EXPECT_EQ(CopyableAndMovable::copy_ctor_called, 0);
  EXPECT_EQ(CopyableAndMovable::move_ctor_called, 0);

  // Reset counters:
  CopyableAndMovable::copy_ctor_called = 0;
  CopyableAndMovable::move_ctor_called = 0;
}

TEST(OnceClosure, AcceptMoveOnlyByValue_PassByRvalueRef) {
  MoveOnly obj;
  EXPECT_EQ(MoveOnly::move_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptMoveOnlyByValue, std::move(obj));

  EXPECT_EQ(MoveOnly::move_ctor_called, 1);
  closure();
  EXPECT_EQ(MoveOnly::move_ctor_called, 2);

  // Reset counters:
  MoveOnly::move_ctor_called = 0;
}

TEST(OnceClosure, AcceptMovableByReference_PassByStdRef) {
  MoveOnly obj;
  EXPECT_EQ(MoveOnly::move_ctor_called, 0);

  OnceClosure closure = BindOnce(AcceptMoveOnlyByReference, std::ref(obj));

  EXPECT_EQ(MoveOnly::move_ctor_called, 0);
  closure();
  EXPECT_EQ(MoveOnly::move_ctor_called, 0);

  // Reset counters:
  MoveOnly::move_ctor_called = 0;
}

}; // namespace base
