#ifndef MAGE_CORE_ENDPOINT_H_
#define MAGE_CORE_ENDPOINT_H_

#include <memory>
#include <string>
#include <queue>

#include "mage/core/message.h"

namespace base {
  class TaskRunner;
}

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
    kBound,
    kUnboundAndQueueing,
    kUnboundAndProxying,
  };

  Endpoint() : state(State::kUnboundAndQueueing) {}
  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;

  void AcceptMessage(Message message);

  // The messages in |incoming_message_queue_| are queued in this endpoint and
  // are waiting to be dispatched to |delegate_| once it is bound. However if
  // this endpoint is being represented by a remote endpoint, someone will want
  // to take these queued messages from us, deliver them to the remote endpoint,
  // and delete us. Once the messages are delivered to the remote endpoint, they
  // are either queued (and might go through this same path), or delivered to
  // its bound |delegate_|.
  std::queue<Message> TakeQueuedMessages();

  void RegisterDelegate(ReceiverDelegate* delegate,
                        std::shared_ptr<base::TaskRunner> delegate_task_runner);

  void UnregisterDelegate();

  void SetProxying(std::string in_node_to_proxy_to);

  std::string name;
  Address peer_address;

  State state;
  std::string node_to_proxy_to;

 private:
  // TODO(domfarolino): Synchronize access to this and `state`.
  // This is used when |delegate_| is null, that is, when this endpoint is not
  // bound to a local interface. We queue the messages here, and then later once
  // bound, these messages will be forwarded, in order, to |delegate_|.
  std::queue<Message> incoming_message_queue_;

  // TODO(domfarolino): Document this.
  std::shared_ptr<base::TaskRunner> delegate_task_runner_;

  // TODO(domfarolino): Document this.
  ReceiverDelegate* delegate_ = nullptr;
};

}; // namspace mage

#endif // MAGE_CORE_ENDPOINT_H_
