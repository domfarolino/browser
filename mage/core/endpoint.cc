#include "mage/core/endpoint.h"

#include "base/threading/thread_checker.h"
#include "mage/core/core.h"

namespace mage {

void Endpoint::AcceptMessage(Message message) {
  printf("Endpoint::AcceptMessage [this=%p]\n", this);
  printf("  name: %s\n", name.c_str());
  printf("  peer_address.node_name: %s\n", peer_address.node_name.c_str());
  printf("  peer_address.endpoint_name: %s\n", peer_address.endpoint_name.c_str());

  // Register all of the endpoints that this message is carrying before we
  // either queue or dispatch it.
  //
  // TODO(domfarolino): I think the fact that we do this whether or not we're
  // queueing or dispatching IS A BUG. It will likely create the same endpoints
  // twice, which is bad. Write a test for this and fix it.
  Pointer<ArrayHeader<EndpointDescriptor>>& endpoints_in_message =
      message.GetMutableMessageHeader().endpoints_in_message;
  if (endpoints_in_message.Get()) {
    int num_endpoints_in_message = endpoints_in_message.Get()->num_elements;
    printf("  num_endpoints_in_message = %d\n", num_endpoints_in_message);
    for (int i = 0; i < num_endpoints_in_message; ++i) {
      // TODO(domfarolino): Use `Message::GetEndpointDescriptors()` instead.
      int byte_offset_for_reading = i * sizeof(EndpointDescriptor);
      EndpointDescriptor& endpoint_descriptor =
          *reinterpret_cast<EndpointDescriptor*>(
              endpoints_in_message.Get()->array_storage() +
              byte_offset_for_reading);
      MageHandle local_handle =
          mage::Core::RecoverMageHandleFromEndpointDescriptor(endpoint_descriptor);
      endpoint_descriptor.Print();
      printf("     Queueing handle to message after recovering endpoint\n");
      message.QueueHandle(local_handle);
    }
  }

  switch (state) {
    case State::kUnboundAndQueueing:
      CHECK(!delegate_);
      printf("Endpoint has accepted a message. Now queueing it\n");
      incoming_message_queue_.push(std::move(message));
      break;
    case State::kBound:
      CHECK(delegate_);
      printf("Endpoint has accepted a message. Now forwarding it to `delegate_` on the delegate's task runner\n");
      // We should consider whether or not we need to be unconditionally posting a
      // task here. If this method is already running on the TaskLoop/thread that
      // `delegate_` is bound to, do we need to post a task at all? It depends on
      // the async semantics that we're going for. We should also test this.
      CHECK(delegate_task_runner_);
      delegate_task_runner_->PostTask(
          base::BindOnce(&ReceiverDelegate::OnReceivedMessage, delegate_,
                         std::move(message)));
      break;
    case State::kUnboundAndProxying:
      printf("Uh-oh, not reached!!!\n");
      NOTREACHED();
      break;
  }
}

std::queue<Message> Endpoint::TakeQueuedMessages() {
  CHECK(!delegate_);
  // TODO(domfarolino): We should also probably set some state so that any
  // more usage of |this| will crash, since after this call, we should be
  // deleted.
  return std::move(incoming_message_queue_);
}

void Endpoint::RegisterDelegate(
    ReceiverDelegate* delegate,
    std::shared_ptr<base::TaskRunner> delegate_task_runner) {
  printf("Endpoint::RegisterDelegate() [this=%p]\n", this);
  printf("state: %d\n", state);
  CHECK_EQ(state, State::kUnboundAndQueueing);
  state = State::kBound;
  // printf("RegisterDelegate() just set state = kBound\n");
  CHECK(!delegate_);
  delegate_ = delegate;
  CHECK(!delegate_task_runner_);
  delegate_task_runner_ = delegate_task_runner;

  printf("  Endpoint::RegisterDelegate seeing if we have messages queued to deliver\n");
  // We may have messages for our `delegate_` already 
  while (!incoming_message_queue_.empty()) {
    printf("  >> Accepting a queued message\n");
    AcceptMessage(std::move(incoming_message_queue_.front()));
    incoming_message_queue_.pop();
  }
}

void Endpoint::UnregisterDelegate() {
  CHECK_EQ(state, State::kBound);
  state = State::kUnboundAndQueueing;
  CHECK(delegate_);
  CHECK(delegate_task_runner_);
  // TODO(domfarolino): Support unregistering a delegate.
}

void Endpoint::SetProxying(std::string in_node_to_proxy_to) {
  CHECK_EQ(state, State::kUnboundAndQueueing);
  state = State::kUnboundAndProxying;
  printf("Endpoint::SetProxying() node_to_proxy_to: %s\n", node_to_proxy_to.c_str());
  node_to_proxy_to = in_node_to_proxy_to;
}

}; // namspace mage
