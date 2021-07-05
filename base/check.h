#ifndef BASE_CHECK_H_
#define BASE_CHECK_H_

#include <assert.h>

#include "base/scheduling/scheduling_handles.h"
#include "base/threading/thread.h"

namespace base {

#define CHECK(condition) assert(condition)
#define CHECK_EQ(actual, expected) CHECK(actual == expected)
#define CHECK_GE(actual, expected) CHECK(actual >= expected)
#define NOTREACHED() CHECK(false)

// Threading and scheduling.
#define CHECK_ON_THREAD(thread_type) \
switch (thread_type) { \
  case ThreadType::UI: \
    CHECK(base::GetCurrentThreadTaskLoop()); \
    CHECK_EQ(base::GetUIThreadTaskLoop(), base::GetCurrentThreadTaskLoop()); \
    break; \
  case ThreadType::IO: \
    CHECK(base::GetCurrentThreadTaskLoop()); \
    CHECK_EQ(base::GetIOThreadTaskLoop(), base::GetCurrentThreadTaskLoop()); \
    break; \
  case ThreadType::WORKER: \
    NOTREACHED(); \
    break; \
}

} // namespace base

#endif // BASE_CHECK_H_
