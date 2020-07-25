#ifndef BASE_WINDOW_WINDOW_H_
#define BASE_WINDOW_WINDOW_H_

namespace base {

class Window {
 public:
  virtual ~Window() {}
  virtual void show() = 0;
};

} // namespace base

#endif // BASE_WINDOW_WINDOW_H_
