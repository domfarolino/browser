#ifndef MAGE_BINDINGS_REMOTE_H_
#define MAGE_BINDINGS_REMOTE_H_

#include "mage/core/handles.h"
#include "mage/core/message.h"

namespace mage {

template <typename Interface>
class Remote {
 public:
  using InterfaceProxy = typename Interface::Proxy;

  Remote() : proxy_(std::make_unique<InterfaceProxy>()) {}

  void Bind(MageHandle local_handle) {
    proxy_->BindToHandle(local_handle);
  }

  MageHandle Unbind() {
    return proxy_->Unbind();
  }

  InterfaceProxy* operator-> () {
    return proxy_.get();
  }

 private:
  std::unique_ptr<InterfaceProxy> proxy_;
};

}; // namspace mage

#endif // MAGE_BINDINGS_REMOTE_H_
