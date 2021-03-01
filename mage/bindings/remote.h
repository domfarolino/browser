#ifndef MAGE_BINDINGS_REMOTE_H_
#define MAGE_BINDINGS_REMOTE_H_

#include <string>
#include <queue>

#include "mage/core/message.h"

namespace mage {

template <typename Interface>
class Remote {
 public:
  Remote() : proxy_(new typename Interface::Proxy()) {}

  typename Interface::Proxy* operator-> () {
    return proxy_;
  }

 private:
  typename Interface::Proxy* proxy_;
};

}; // namspace mage

#endif // MAGE_BINDINGS_REMOTE_H_
