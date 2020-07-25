#ifndef MAINWINDOW_X11_H_
#define MAINWINDOW_X11_H_

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include <memory>

#include "base/window/window.h"

namespace base {

class WindowLinux : public base::Window {
 public:
  WindowLinux();
  ~WindowLinux();
  void show();

 private:
  Display* display_;
};

} // namespace base

#endif // MAINWINDOW_X11_H_
