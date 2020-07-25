#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include "iwindow.h"

namespace base {

class MainWindow : base::IWindow {
public:
  MainWindow();
  virtual ~MainWindow();
  void show();

private:
  IWindow* platform_window_;
};

} // namespace base

#endif // MAINWINDOW_H_
