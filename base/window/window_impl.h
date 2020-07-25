#ifndef BASE_WINDOW_WINDOW_IMPL_H_
#define BASE_WINDOW_WINDOW_IMPL_H_

#include <memory>

#include "base/window/window.h"

namespace base {

class WindowImpl final : public base::Window {
 public:
  WindowImpl();
  virtual ~WindowImpl();
  void Show() override;

 private:
  std::unique_ptr<Window> platform_window_;
};

} // namespace base

#endif // BASE_WINDOW_WINDOW_IMPL_H_
