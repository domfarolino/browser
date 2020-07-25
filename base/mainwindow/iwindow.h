#ifndef IWINDOW_H_
#define IWINDOW_H_

namespace base {

class IWindow {
public:
  virtual ~IWindow() {}
  virtual void show() = 0;
};

}

#endif // IWINDOW_H_
