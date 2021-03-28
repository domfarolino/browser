#ifndef MAGE_BINDINGS_RECEIVER_H_
#define MAGE_BINDINGS_RECEIVER_H_

#include "mage/core/handles.h"
#include "mage/core/message.h"

namespace mage {

template <typename Interface>
class Receiver {
 public:
  using InterfaceStub = typename Interface::ReceiverStub;

  Receiver() = default;

  void Bind(MageHandle local_handle, Interface* impl) {
    impl_ = impl;
    stub_ = new InterfaceStub();
    stub_->BindToHandle(local_handle, impl);
  }

  // TODO(domfarolino): We probably want some way to unbind handles from
  // mage::Receivers.

 private:
  // This is the implementation of the mage interface that we'll ultimately
  // dispatch messages to.
  Interface* impl_;

  // This is the receiver stub that once bound to a |MageHandle|, associates
  // itself with the underlying |mage::Endpoint| that receives messages bound
  // for |impl_|. It is a generated object that automatically deserializes the
  // message data and dispatches the correct method on |impl_|.
  InterfaceStub* stub_;
};

}; // namspace mage

#endif // MAGE_BINDINGS_RECEIVER_H_
