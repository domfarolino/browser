#include "mage/core/endpoint.h"

#include "base/scheduling/task_runner.h"
#include "base/threading/thread_checker.h"
#include "mage/core/core.h"

namespace mage {

namespace {
  // `Endpoint` occassionally wants to verify whether a `std::weak_ptr` has been
  // assigned to some object without asserting anything about the object's
  // lifetime. This helper from https://stackoverflow.com/a/45507610/3947332
  // enables this.
  template <typename T>
  bool is_weak_ptr_assigned(std::weak_ptr<T> const& weak) {
      using wt = std::weak_ptr<T>;
      return weak.owner_before(wt{}) || wt{}.owner_before(weak);
  }
}  // namespace

// Guarded by `lock_`.
void Endpoint::AcceptMessageOnIOThread(Message message) {
  CHECK_ON_THREAD(base::ThreadType::IO);
  CHECK(state != State::kUnboundAndProxying);
  printf("Endpoint::AcceptMessageOnIOThread [this=%p] [pid=%d]\n", this, getpid());
  printf("  name: %s\n", name.c_str());
  printf("  state: %d\n", (int)state);
  printf("  peer_address.node_name: %s\n", peer_address.node_name.c_str());
  printf("  peer_address.endpoint_name: %s\n", peer_address.endpoint_name.c_str());

  AcceptMessage(std::move(message));
}
// Guarded by `lock_`.
void Endpoint::AcceptMessageOnDelegateThread(Message message) {
  CHECK(state != State::kUnboundAndProxying);
  printf("Endpoint::AcceptMessageOnDelegateThread [this=%p] [pid=%d]\n", this, getpid());
  printf("  name: %s\n", name.c_str());
  printf("  state: %d\n", (int)state);
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
  printf("  state: %d\n", (int)state);
  printf("  peer_address.node_name: %s\n", peer_address.node_name.c_str());
  printf("  peer_address.endpoint_name: %s\n", peer_address.endpoint_name.c_str());
  printf("  number_of_handles: %d\n", message.NumberOfHandles());

  switch (state) {
    case State::kUnboundAndQueueing:
      CHECK(!is_weak_ptr_assigned(delegate_));
      printf("  Endpoint is queueing a message to `incoming_message_queue_`\n");
      incoming_message_queue_.push(std::move(message));
      break;
    case State::kBound:
      CHECK(is_weak_ptr_assigned(delegate_));
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

// Guarded by `lock_`.
void Endpoint::PostMessageToDelegate(Message message) {
  CHECK_EQ(state, State::kBound);
  CHECK(is_weak_ptr_assigned(delegate_));
  CHECK(delegate_task_runner_);
  // We should consider whether or not we need to be unconditionally posting a
  // task here. If this method is already running on the TaskLoop/thread that
  // `delegate_` is bound to, do we need to post a task at all? It depends on
  // the async semantics that we're going for. We should also test this.
  delegate_task_runner_->PostTask(
      base::BindOnce(&ReceiverDelegate::DispatchMessageIfStillAlive, delegate_,
                     std::move(message)));
}

std::queue<Message> Endpoint::TakeQueuedMessages() {
  CHECK(!is_weak_ptr_assigned(delegate_));
  std::queue<Message> messages_to_return = std::move(incoming_message_queue_);
  return messages_to_return;
}

void Endpoint::RegisterDelegate(
    std::weak_ptr<ReceiverDelegate> delegate,
    std::shared_ptr<base::TaskRunner> delegate_task_runner) {
  // Before we observe our state and incoming message queue, we need to grab a
  // lock in case another thread is modifying this state.
  Lock();

  printf("Endpoint::RegisterDelegate() [this=%p] [getpid=%d]\n", this, getpid());
  printf("state: %d\n", (int)state);
  CHECK_EQ(state, State::kUnboundAndQueueing);
  state = State::kBound;
  // printf("RegisterDelegate() just set state = kBound\n");
  CHECK(!is_weak_ptr_assigned(delegate_));
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

// Not implemented. See header documentation.
void Endpoint::UnregisterDelegate() {
  NOTREACHED();
  CHECK_EQ(state, State::kBound);
  state = State::kUnboundAndQueueing;
  CHECK(is_weak_ptr_assigned(delegate_));
  CHECK(delegate_task_runner_);
}

void Endpoint::SetProxying(std::string in_node_name, std::string in_endpoint_name) {
  CHECK_EQ(state, State::kUnboundAndQueueing);
  state = State::kUnboundAndProxying;
  proxy_target.node_name = in_node_name;
  proxy_target.endpoint_name = in_endpoint_name;
  printf("Endpoint::SetProxying() proxy_target: (%s, %s):\n", proxy_target.node_name.c_str(), proxy_target.endpoint_name.c_str());
}

}; // namspace mage
