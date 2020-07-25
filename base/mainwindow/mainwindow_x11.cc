#include "mainwindow_x11.h"

namespace base {

MainWindow_X11::MainWindow_X11() {
  this->display_ = XOpenDisplay(0);

  assert(display_);

  int blackColor = BlackPixel(display_, DefaultScreen(display_));
  int whiteColor = WhitePixel(display_, DefaultScreen(display_));

  ::Window w = XCreateSimpleWindow(display_, DefaultRootWindow(display_), 0, 0,
                                 200, 100, 0, blackColor, blackColor);

  XSelectInput(display_, w, StructureNotifyMask | KeyPressMask);

  XMapWindow(display_, w);

  GC gc = XCreateGC(display_, w, 0, 0);

  XSetForeground(display_, gc, whiteColor);
}

MainWindow_X11::~MainWindow_X11() {}

void MainWindow_X11::show() {
  printf("creating window - press `q` to quit\n");
  while (1) {
    XEvent e;
    XNextEvent(display_, &e);
    if (e.type == KeyPress) {
      if (e.xkey.keycode == 24){
       break;
      }
      printf("state:%d\tkeycode:%d\n", e.xkey.state, e.xkey.keycode);
      fflush(stdout);
    }
    else {
      printf("event type: %d\n", e.type);
      fflush(stdout);
    }
    XFlush(display_);
    fflush(stdout);
  }
}

} // namespace base
