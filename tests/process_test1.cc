#include "../base/synchronization.h"

#define CHECK(condition) assert(condition)
#define CHECK_EQ(actual, expected) CHECK(actual == expected)
#define NOT_REACHED() CHECK(false)

namespace test {

void test1() {
  Mutex m = new Mutex();
  m.lock();
  m.unlock();
}

} // namespace test 

