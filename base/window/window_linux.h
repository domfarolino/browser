#ifndef BASE_WINDOW_WINDOW_LINUX_H_
#define BASE_WINDOW_WINDOW_LINUX_H_

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <memory>

#include "base/window/window.h"

namespace base {

class WindowLinux final : public base::Window {
 public:
  WindowLinux();
  ~WindowLinux();
  void Show() override;

 private:
  Display* display_;
};

} // namespace base

#endif // BASE_WINDOW_WINDOW_LINUX_H_
