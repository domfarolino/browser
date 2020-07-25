#include "base/window/window_linux.h"

namespace base {

// WindowLinux::WindowLinux() : display_(XOpenDisplay(0)) {
WindowLinux::WindowLinux() {
  this->display_ = XOpenDisplay(0);
  int black_color = BlackPixel(display_, DefaultScreen(display_));
  int white_color = WhitePixel(display_, DefaultScreen(display_));

  ::Window w = XCreateSimpleWindow(display_, DefaultRootWindow(display_), 0, 0,
                                 200, 100, 0, black_color, black_color);

  XSelectInput(display_, w, StructureNotifyMask | KeyPressMask);

  XMapWindow(display_, w);

  GC gc = XCreateGC(display_, w, 0, 0);

  XSetForeground(display_, gc, white_color);
}

WindowLinux::~WindowLinux() {}

void WindowLinux::Show() {
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
  XCloseDisplay(display_);
}

} // namespace base
