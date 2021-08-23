#ifndef MAGE_CORE_ENDPOINT_H_
#define MAGE_CORE_ENDPOINT_H_

#include <memory>
#include <string>
#include <functional>
#include <queue>

// TODO(domfarolino): Remove this.
#include "base/scheduling/task_loop.h"
#include "mage/core/message.h"

namespace mage {

struct Address {
  std::string node_name;
  std::string endpoint_name;
};

class Endpoint {
 public:
  class ReceiverDelegate {
   public:
    virtual ~ReceiverDelegate() = default;
    virtual void OnReceivedMessage(Message) = 0;
  };

  enum class State {
    kUnbound, // Queueing
    kBound,
  };

  Endpoint() : state(State::kUnbound) {}
  /*
  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;
  */

  // TODO(domfarolino): Don't keep this inline.
  void AcceptMessage(Message message) {
    if (!delegate_) {
      printf("Endpoint has accepted a message. Now queueing it\n");
      incoming_message_queue_.push(std::move(message));
      return;
    }

    printf("Endpoint has accepted a message. Now forwarding it\n");

    ReceiverDelegate* dummy_delegate = nullptr;
    // TODO(domfarolino): This is messy, let's do this the right way.
    auto task_1 = std::bind(&Endpoint::AcceptMessage, this, std::move(message));
    //std::function<void()> task = std::bind(&Endpoint::RegisterDelegate, this, dummy_delegate);

    // delegate_->OnReceivedMessage(std::move(message));
  }

  // The messages in |incoming_message_queue_| are queued in this endpoint and
  // are waiting to be dispatched to |delegate_| once it is bound. However if
  // this endpoint is being represented by a remote endpoint, someone will want
  // to take these queued messages from us, deliver them to the remote endpoint,
  // and delete us. Once the messages are delivered to the remote endpoint, they
  // are either queued (and might go through this same path), or delivered to
  // its bound |delegate_|.
  std::queue<Message> TakeQueuedMessages() {
    // TODO(domfarolino): We probably want to check that we're not currently
    // bound to a delegate etc.
    // TODO(domfarolino): We should also probably set some state so that any
    // more usage of |this| will crash, since after this call, we should be
    // deleted.
    return std::move(incoming_message_queue_);
  }

  void RegisterDelegate(ReceiverDelegate* delegate) {
    CHECK(!delegate_);
    delegate_ = delegate;
  }

  void UnregisterDelegate() {
    // TODO(domfarolino): Support unregistering a delegate.
  }

  std::string name;
  Address peer_address;

  State state;

 private:
  // This is used when |delegate_| is null, that is, when this endpoint is not
  // bound to a local interface. We queue the messages here, and then later once
  // bound, these messages will be forwarded, in order, to |delegate_|.
  std::queue<Message> incoming_message_queue_;

  // TODO(domfarolino): Document this.
  ReceiverDelegate* delegate_ = nullptr;
};

}; // namspace mage

#endif // MAGE_CORE_ENDPOINT_H_
