#ifndef MAGE_BINDINGS_REMOTE_H_
#define MAGE_BINDINGS_REMOTE_H_

#include "mage/core/handles.h"
#include "mage/core/message.h"

namespace mage {

template <typename Interface>
class Remote {
 public:
  using InterfaceProxy = typename Interface::Proxy;

  Remote() : proxy_(new InterfaceProxy()) {}

  void Bind(MageHandle local_handle) {
    proxy_->BindToHandle(local_handle);
  }

  // TODO(domfarolino): We probably want some way to unbind handles from
  // mage::Remotes.

  InterfaceProxy* operator-> () {
    return proxy_;
  }

 private:
  InterfaceProxy* proxy_;
};

}; // namspace mage

#endif // MAGE_BINDINGS_REMOTE_H_
