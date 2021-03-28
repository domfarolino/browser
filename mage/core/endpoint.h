#ifndef MAGE_CORE_ENDPOINT_H_
#define MAGE_CORE_ENDPOINT_H_

#include <memory>
#include <string>
#include <queue>

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
  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;

  // TODO(domfarolino): Don't keep this inline.
  void AcceptMessage(Message message) {
    printf("Endpoint has accepted a message. Now forwarding it\n");
    // TODO(domfarolino): Support message queueing via
    // |incoming_message_queue_|. But for now, we always rely on their being a
    // local delegate that we can forward the |message| to.
    CHECK(delegate_);
    delegate_->OnReceivedMessage(std::move(message));
  }

  void RegisterDelegate(ReceiverDelegate* delegate) {
    delegate_ = delegate;
  }

  std::string name;
  Address peer_address;

  State state;

 private:
  // This is used when |delegate_| is null, that is, when this endpoint is not
  // bound to a local interface. We queue the messages here, and then later once
  // bound, these messages will be forwarded, in order, to |delegate_|.
  std::queue<std::unique_ptr<Message>> incoming_message_queue_;

  // TODO(domfarolino): Document this.
  // TODO(domfarolino): This should not be a raw pointer.
  ReceiverDelegate* delegate_;
};

}; // namspace mage

#endif // MAGE_CORE_ENDPOINT_H_
