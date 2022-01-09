#ifndef MAGE_BINDINGS_RECEIVER_H_
#define MAGE_BINDINGS_RECEIVER_H_

#include "base/scheduling/scheduling_handles.h"
#include "base/threading/thread_checker.h"
#include "mage/core/handles.h"
#include "mage/core/message.h"

namespace mage {

template <typename Interface>
class Receiver {
 public:
  using InterfaceStub = typename Interface::ReceiverStub;

  Receiver() = default;

  void Bind(MageHandle local_handle, Interface* impl) {
    CHECK(thread_checker_.CalledOnConstructedThread());
    impl_ = impl;
    stub_ = new InterfaceStub();
    // We pass in the current thread's `base::TaskRunner` so that mage knows
    // which task runner to dispatch messages to `impl_` on.
    stub_->BindToHandle(local_handle, impl, base::GetCurrentThreadTaskRunner());
  }

  // TODO(domfarolino): We probably want some way to unbind handles from
  // mage::Receivers.

 private:
  // This is the user-provided implementation of the mage interface that we'll
  // ultimately dispatch messages to.
  Interface* impl_;

  // This is the receiver stub that once bound to a |MageHandle|, associates
  // itself with the underlying |mage::Endpoint| that receives messages bound
  // for |impl_|. It is a generated object that automatically deserializes the
  // message data and dispatches the correct method on |impl_|.
  InterfaceStub* stub_;

  // If we ever have the use-case of creating a receiver on one thread and
  // binding it on another, then we can remove this. Until then however, this is
  // the safest way to maintain this object.
  base::ThreadChecker thread_checker_;
};

}; // namspace mage

#endif // MAGE_BINDINGS_RECEIVER_H_
