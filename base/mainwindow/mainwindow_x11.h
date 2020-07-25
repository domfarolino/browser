#ifndef MAINWINDOW_X11_H_
#define MAINWINDOW_X11_H_

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "iwindow.h"

namespace base {

class MainWindow_X11 : public base::IWindow {
public:
  MainWindow_X11();
  ~MainWindow_X11();
  void show();

private:
  Display* display_;
};

} // namespace base

#endif // MAINWINDOW_X11_H_
