#include "mainwindow.h"
#ifdef __APPLE__
#include "base/mainwindow/mainwindow_mac.h"
#elif __linux__
#include "base/mainwindow/mainwindow_x11.h"
#elif _WIN32
#include "base/mainwindow/mainwindow_win32.h"
#endif

namespace base {

MainWindow::MainWindow() {
#ifdef __APPLE__

#elif __linux__
  this->platform_window_ = new MainWindow_X11();
#elif _WIN32

#endif
}

MainWindow::~MainWindow() {
  delete platform_window_;
}

void MainWindow::show() {
  platform_window_->show();
}

} // namespace base
