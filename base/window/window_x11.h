#ifndef WINDOW_X11_H_
#define WINDOW_X11_H_

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

namespace base {

class MainWindow {
public:
  MainWindow();
  ~MainWindow();
  void show();

private:
  Display* display;
};

} // namespace base

#endif // WINDOW_X11_H_
