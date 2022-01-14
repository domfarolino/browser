#ifndef MAGE_CORE_ENDPOINT_H_
#define MAGE_CORE_ENDPOINT_H_

#include <memory>
#include <string>
#include <functional>
#include <queue>

// TODO(domfarolino): Remove this.
#include "base/scheduling/scheduling_handles.h"
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
    kBound,
    kUnboundAndQueueing,
    kUnboundAndProxying,
  };

  Endpoint() : state(State::kUnboundAndQueueing) {}
  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;

  // TODO(domfarolino): Don't keep this inline.
  // TODO(domfarolino): We should probably ensure this method is always being
  // called on the UI thread. At least before checking `delegate_`, because that
  // is not thread-safe.
  void AcceptMessage(Message message) {
    printf("Endpoint::AcceptMessage\n");
    printf("  name: %s\n  peer_address.node_name: %s\n  peer_address.endpoint_name: %s\n", name.c_str(), peer_address.node_name.c_str(), peer_address.endpoint_name.c_str());
    if (state == State::kUnboundAndQueueing) {
      CHECK(!delegate_);
      printf("Endpoint has accepted a message. Now queueing it\n");
      incoming_message_queue_.push(std::move(message));
      return;
    }

    if (state == State::kBound) {
      CHECK(delegate_);
      printf("Endpoint has accepted a message. Now forwarding it to `delegate_`\n");
      // We should consider whether or not we need to be unconditionally posting a
      // task here. If this method is already running on the TaskLoop/thread that
      // `delegate_` is bound to, do we need to post a task at all? It depends on
      // the async semantics that we're going for. We should also test this.
      CHECK(delegate_task_runner_);
      delegate_task_runner_->PostTask(
          base::BindOnce(&ReceiverDelegate::OnReceivedMessage, delegate_,
                         std::move(message)));
      return;
    }

    printf("Uh-oh, not reached!!!\n");
    NOTREACHED();
  }

  // The messages in |incoming_message_queue_| are queued in this endpoint and
  // are waiting to be dispatched to |delegate_| once it is bound. However if
  // this endpoint is being represented by a remote endpoint, someone will want
  // to take these queued messages from us, deliver them to the remote endpoint,
  // and delete us. Once the messages are delivered to the remote endpoint, they
  // are either queued (and might go through this same path), or delivered to
  // its bound |delegate_|.
  std::queue<Message> TakeQueuedMessages() {
    CHECK(!delegate_);
    // TODO(domfarolino): We should also probably set some state so that any
    // more usage of |this| will crash, since after this call, we should be
    // deleted.
    return std::move(incoming_message_queue_);
  }

  void RegisterDelegate(ReceiverDelegate* delegate,
                        std::shared_ptr<base::TaskRunner> delegate_task_runner) {
    printf("Endpoint::RegisterDelegate() %p\n", this);
    printf("state: %d\n", state);
    CHECK_EQ(state, State::kUnboundAndQueueing);
    state = State::kBound;
    CHECK(!delegate_);
    delegate_ = delegate;
    CHECK(!delegate_task_runner_);
    delegate_task_runner_ = delegate_task_runner;

    printf("  Endpoint::RegisterDelegate seeing if we have messages queued to deliver\n");
    // We may have messages for our `delegate_` already 
    while (!incoming_message_queue_.empty()) {
      printf("Endpoint::RegisterDelegate >> accepting a queued message\n");
      AcceptMessage(std::move(incoming_message_queue_.front()));
      incoming_message_queue_.pop();
    }
  }

  void UnregisterDelegate() {
    CHECK_EQ(state, State::kBound);
    state = State::kUnboundAndQueueing;
    CHECK(delegate_);
    CHECK(delegate_task_runner_);
    // TODO(domfarolino): Support unregistering a delegate.
  }

  void SetProxying(std::string in_node_to_proxy_to) {
    CHECK_EQ(state, State::kUnboundAndQueueing);
    state = State::kUnboundAndProxying;
    printf("Endpoint::SetProxying() node_to_proxy_to: %s\n", node_to_proxy_to.c_str());
    node_to_proxy_to = in_node_to_proxy_to;
  }

  std::string name;
  Address peer_address;

  State state;
  std::string node_to_proxy_to;

 private:
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
