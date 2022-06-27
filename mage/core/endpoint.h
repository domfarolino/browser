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

  // This can be called on:
  //   - The IO thread, by `AcceptMessageOnIOThread()` (see below)
  //   - The UI or a worker thread, when receiving a message from a local
  //     endpoint. This kind of message went through the `peer_is_local` path in
  //     `Node::SendMessage()`. This is also called for each message in
  //     `incoming_message_queue_` when `delegate_` is bound (i.e., going from
  //     `kUnboundAndQueueing` state to `kBound`).
  // In both of these cases, `state` is either:
  //   - kBound: The message will immediately be dispatched to the underlying
  //    `delegate_`.
  //   - kUnboundAndQueueing: The message will be queued in
  //     `incoming_message_queues_` and will be replayed to `delegate_` once
  //     bound.
  // TODO(domfarolino): It seems possible for this to be called for a node in
  // the proxying state. We should verify this and fix this. Probably messages
  // received for nodes in the proxying state should be handled by `Node`.
  void AcceptMessage(Message message);

  // This method is called by `Node` on the IO thread. It is only called when a
  // message is received from a remote node/process via `Channel`. When this is
  // called, `state` is either:
  //   - kBound: We post a task to deliver this message to `delegate_`
  //   - kUnboundAndQueueing: The message will be queued to
  //     `incoming_message_queue_` and replayed to `delegate_` once bound, if
  //     ever.
  // This method always processes the `EndpointDescriptors` in `message` if any,
  // by registering them with the local `Node`. We expect that the endpoints in
  // this message do not exist until we create them. They might already exist if
  // the endpoints are traveling back to a process they've already been at
  // before, but we do not support that use-case currently.
  void AcceptMessageOnIOThread(Message message);

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
