#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#include <functional>
#include <tuple>

// Source: https://stackoverflow.com/questions/16868129.

namespace base {

using Callback = std::function<void()>;
using Predicate = std::function<bool()>;

} // namespace base

#endif // BASE_CALLBACK_H_
