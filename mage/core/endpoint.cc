#include "mage/core/endpoint.h"

#include "base/threading/thread_checker.h"
#include "mage/core/core.h"

namespace mage {

// Guarded by `lock_`.
void Endpoint::AcceptMessageOnIOThread(Message message) {
  CHECK_ON_THREAD(base::ThreadType::IO);
  CHECK(state != State::kUnboundAndProxying);
  printf("Endpoint::AcceptMessageOnIOThread [this=%p] [pid=%d]\n", this, getpid());
  printf("  name: %s\n", name.c_str());
  printf("  state: %d\n", state);
  printf("  peer_address.node_name: %s\n", peer_address.node_name.c_str());
  printf("  peer_address.endpoint_name: %s\n", peer_address.endpoint_name.c_str());

  AcceptMessage(std::move(message));
}
// Guarded by `lock_`.
void Endpoint::AcceptMessageOnDelegateThread(Message message) {
  CHECK(state != State::kUnboundAndProxying);
  printf("Endpoint::AcceptMessageOnDelegateThread [this=%p] [pid=%d]\n", this, getpid());
  printf("  name: %s\n", name.c_str());
  printf("  state: %d\n", state);
  printf("  peer_address.node_name: %s\n", peer_address.node_name.c_str());
  printf("  peer_address.endpoint_name: %s\n", peer_address.endpoint_name.c_str());

  // Process and register all of the endpoints that `message` is carrying before
  // we either queue or dispatch it.
  std::vector<EndpointDescriptor> endpoints_in_message = message.GetEndpointDescriptors();
  printf("  endpoints_in_message.size()= %lu\n", endpoints_in_message.size());
  for (const EndpointDescriptor& endpoint_descriptor : endpoints_in_message) {
    MageHandle local_handle =
        mage::Core::RecoverExistingMageHandleFromEndpointDescriptor(endpoint_descriptor);
    endpoint_descriptor.Print();
    printf("     Queueing handle to message after recovering endpoint\n");
    message.QueueHandle(local_handle);
  }

  AcceptMessage(std::move(message));
}

// Guarded by `lock_`.
void Endpoint::AcceptMessage(Message message) {
  CHECK(state != State::kUnboundAndProxying);
  printf("Endpoint::AcceptMessage() [this=%p], [pid=%d]\n", this, getpid());
  printf("  name: %s\n", name.c_str());
  printf("  state: %d\n", state);
  printf("  peer_address.node_name: %s\n", peer_address.node_name.c_str());
  printf("  peer_address.endpoint_name: %s\n", peer_address.endpoint_name.c_str());
  printf("  number_of_handles: %d\n", message.NumberOfHandles());

  switch (state) {
    case State::kUnboundAndQueueing:
      CHECK(!delegate_);
      printf("  Endpoint is queueing a message to `incoming_message_queue_`\n");
      incoming_message_queue_.push(std::move(message));
      break;
    case State::kBound:
      CHECK(delegate_);
      printf("  Endpoint has accepted a message. Now forwarding it to `delegate_` on the delegate's task runner\n");
      PostMessageToDelegate(std::move(message));
      break;
    case State::kUnboundAndProxying:
      // `Endpoint` is never responsible for handling messages when it is in the
      // proxying state. That should be handled at the layer above us.
      NOTREACHED();
      break;
  }
  printf("Endpoint::AcceptMessage() DONE\n");
}

void Endpoint::PostMessageToDelegate(Message message) {
  CHECK_EQ(state, State::kBound);
  CHECK(delegate_);
  CHECK(delegate_task_runner_);
  // We should consider whether or not we need to be unconditionally posting a
  // task here. If this method is already running on the TaskLoop/thread that
  // `delegate_` is bound to, do we need to post a task at all? It depends on
  // the async semantics that we're going for. We should also test this.
  delegate_task_runner_->PostTask(
      base::BindOnce(&ReceiverDelegate::OnReceivedMessage, delegate_,
                     std::move(message)));
}

std::queue<Message> Endpoint::TakeQueuedMessages() {
  Lock();
  CHECK(!delegate_);
  // TODO(domfarolino): We should also probably set some state so that any
  // more usage of |this| will crash, since after this call, we should be
  // deleted.
  std::queue<Message> messages_to_return = std::move(incoming_message_queue_);
  Unlock();
  return messages_to_return;
}

void Endpoint::RegisterDelegate(
    ReceiverDelegate* delegate,
    std::shared_ptr<base::TaskRunner> delegate_task_runner) {
  // Before we observe our state and incoming message queue, we need to grab a
  // lock in case another thread is modifying this state.
  Lock();

  printf("Endpoint::RegisterDelegate() [this=%p] [getpid=%d]\n", this, getpid());
  printf("state: %d\n", state);
  CHECK_EQ(state, State::kUnboundAndQueueing);
  state = State::kBound;
  // printf("RegisterDelegate() just set state = kBound\n");
  CHECK(!delegate_);
  delegate_ = delegate;
  CHECK(!delegate_task_runner_);
  delegate_task_runner_ = delegate_task_runner;

  printf("  Endpoint::RegisterDelegate() seeing if we have queued messages to deliver\n");
  // We may have messages queued up for our `delegate_` already .
  while (!incoming_message_queue_.empty()) {
    printf("    >> Found a message; calling PostMessageToDelegate() to forward the queued message to delegate\n");
    PostMessageToDelegate(std::move(incoming_message_queue_.front()));
    incoming_message_queue_.pop();
  }
  printf("  Endpoint::RegisterDelegate() done delivering messages \n");
  Unlock();
}

void Endpoint::UnregisterDelegate() {
  CHECK_EQ(state, State::kBound);
  state = State::kUnboundAndQueueing;
  CHECK(delegate_);
  CHECK(delegate_task_runner_);
  // TODO(domfarolino): Support unregistering a delegate.
}

void Endpoint::SetProxying(std::string in_node_name, std::string in_endpoint_name) {
  CHECK_EQ(state, State::kUnboundAndQueueing);
  state = State::kUnboundAndProxying;
  proxy_target.node_name = in_node_name;
  proxy_target.endpoint_name = in_endpoint_name;
  printf("Endpoint::SetProxying() proxy_target: (%s, %s):\n", proxy_target.node_name.c_str(), proxy_target.endpoint_name.c_str());
}

}; // namspace mage
