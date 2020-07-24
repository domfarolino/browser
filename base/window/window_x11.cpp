#include "window_x11.h"

namespace base {

MainWindow::MainWindow() {
  this->display = XOpenDisplay(0);

  assert(display);

  int blackColor = BlackPixel(display, DefaultScreen(display));
  int whiteColor = WhitePixel(display, DefaultScreen(display));

  Window w = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0,
                                 200, 100, 0, blackColor, blackColor);

  XSelectInput(display, w, StructureNotifyMask | KeyPressMask);

  XMapWindow(display, w);

  GC gc = XCreateGC(display, w, 0, 0);

  XSetForeground(display, gc, whiteColor);
}

MainWindow::~MainWindow() {}

void MainWindow::show() {
  while (1) {
    XEvent e;
    XNextEvent(display, &e);
    if (e.type == KeyPress) {
      printf("state:%d\tkeycode:%d\n", e.xkey.state, e.xkey.keycode);
      fflush(stdout);
    }
    else {
      printf("event type: %d\n", e.type);
      fflush(stdout);
    }
    XFlush(display);
    fflush(stdout);
  }
}

} // namespace base
