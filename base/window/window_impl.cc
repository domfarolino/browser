#include "base/build_config.h"
#include "base/window/window.h"
#include "base/window/window_impl.h"
#ifdef OS_MACOS
#include "base/window/window_macos.h"
#elif OS_LINUX
#include "base/window/window_linux.h"
#endif

namespace base {

#ifdef OS_MACOS
WindowImpl::WindowImpl() : platform_window_() {}
#elif OS_LINUX
WindowImpl::WindowImpl() : platform_window_(new WindowLinux()) {}
#endif

WindowImpl::~WindowImpl() {}

void WindowImpl::Show() {
  platform_window_->Show();
}

} // namespace base
