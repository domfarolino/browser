#ifndef BASE_CHECK_H_
#define BASE_CHECK_H_

#include <assert.h>

namespace base {

#define CHECK(condition) assert(condition)
#define CHECK_EQ(actual, expected) CHECK(actual == expected)
#define NOT_REACHED() CHECK(false)

} // namespace base

#endif // BASE_CHECK_H_
