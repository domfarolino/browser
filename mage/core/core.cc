#include "mage/core/core.h"

#include <unistd.h>
#include <cstdlib>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/endpoint.h"
#include "mage/core/node.h"
#include "mage/core/util.h"

namespace mage {

Core* g_core = nullptr;

Core::Core()
    : origin_task_runner_(base::GetCurrentThreadTaskRunner()),
      node_(new Node()) {}

// static
void Core::Init() {
  srand(getpid());

  CHECK(!g_core);
  g_core = new Core();
}

// static
void Core::ShutdownCleanly() {
  delete g_core;
  g_core = nullptr;
}

// static
Core* Core::Get() {
  return g_core;
}

// static
std::vector<MageHandle> Core::CreateMessagePipes() {
  std::vector<MageHandle> return_handles = Get()->node_->CreateMessagePipes();
  CHECK_NE(Get()->handle_table_.find(return_handles[0]),
           Get()->handle_table_.end());
  CHECK_NE(Get()->handle_table_.find(return_handles[1]),
           Get()->handle_table_.end());
  return return_handles;
}

// static
MageHandle Core::SendInvitationAndGetMessagePipe(int fd,
                                                 base::OnceClosure callback) {
  Get()->remote_has_accepted_invitation_callback_ = std::move(callback);
  return Get()->node_->SendInvitationAndGetMessagePipe(fd);
}

// static
void Core::AcceptInvitation(
    int fd,
    std::function<void(MageHandle)> finished_accepting_invitation_callback) {
  Get()->finished_accepting_invitation_callback_ =
      std::move(finished_accepting_invitation_callback);
  Get()->node_->AcceptInvitation(fd);
}

// static
void Core::SendMessage(MageHandle local_handle, Message message) {
  auto endpoint_it = Get()->handle_table_.find(local_handle);
  printf("Core::SendMessage\n");
  CHECK_NE(endpoint_it, Get()->handle_table_.end());
  Get()->node_->SendMessage(endpoint_it->second, std::move(message));
}

// static
void Core::BindReceiverDelegateToEndpoint(
    MageHandle local_handle,
    std::weak_ptr<Endpoint::ReceiverDelegate> delegate,
    std::shared_ptr<base::TaskRunner> delegate_task_runner) {
  auto endpoint_it = Get()->handle_table_.find(local_handle);
  CHECK_NE(endpoint_it, Get()->handle_table_.end());
  std::shared_ptr<Endpoint> endpoint = endpoint_it->second;
  endpoint->RegisterDelegate(delegate, std::move(delegate_task_runner));
}

// static
void Core::PopulateEndpointDescriptor(
    MageHandle handle_to_send,
    MageHandle handle_of_preexisting_connection,
    EndpointDescriptor& endpoint_descriptor_to_populate) {
  std::shared_ptr<Endpoint> local_endpoint_of_preexisting_connection =
      Get()->handle_table_.find(handle_of_preexisting_connection)->second;
  CHECK(local_endpoint_of_preexisting_connection);
  // This path can only be hit when you have a direct handle to an endpoint,
  // which is only possible if the endpoint backing the handle is not proxying.
  CHECK_NE(local_endpoint_of_preexisting_connection->state,
           Endpoint::State::kUnboundAndProxying);

  std::string peer_node_name =
      local_endpoint_of_preexisting_connection->peer_address.node_name;
  std::string peer_endpoint_name =
      local_endpoint_of_preexisting_connection->peer_address.endpoint_name;

  printf(
      "**************PopulateEndpointDescriptor() populating "
      "EndpointDescriptor:\n");
  printf("    'sending' endpoint name: %s\n",
         local_endpoint_of_preexisting_connection->name.c_str());
  printf("    'sending' endpoint [peer node: %s]\n", peer_node_name.c_str());
  printf("    'sending' endpoint [peer endpoint: %s]\n",
         peer_endpoint_name.c_str());

  // Populating an `EndpointDescriptor` is easy regardless of whether it is
  // being sent same-process or cross-process.
  //   1.) Fill out the name of the endpoint that we are sending. This is used
  //       in case the descriptor is sent to a same-process endpoint, in which
  //       case we don't actually create a "new" endpoint from this
  //       descriptor, but we know to just target the already-existing one
  //       with this name.
  //   2.) Generate and fill out a new `cross_node_endpoint_name`: this is
  //       used as the target endpoint's name upon endpoint creation if the
  //       descriptor is sent cross-process. We generate this up-front so that
  //       if the descriptor does go cross-process, the sending endpoint
  //       automatically knows the name by which to target the remote
  //       endpoint. This name is used in the `proxy_target` of the endpoint
  //       (in *this* process) that we're "sending".
  // TODO(domfarolino): I think there is a bug with the following two steps.
  // See the test case in https://github.com/domfarolino/browser/pull/32 that
  // starts with "Process A creates two endpoints.
  //   3.) The target endpoint's peer node name when it lives in another other
  //       process is just the current endpoint's peer node name.
  //   4.) Same as (3), for the peer's endpoint name.
  std::shared_ptr<Endpoint> endpoint_being_sent =
      Get()->handle_table_.find(handle_to_send)->second;
  memcpy(endpoint_descriptor_to_populate.endpoint_name,
         endpoint_being_sent->name.c_str(), kIdentifierSize);
  std::string cross_node_endpoint_name = util::RandomIdentifier();
  memcpy(endpoint_descriptor_to_populate.cross_node_endpoint_name,
         cross_node_endpoint_name.c_str(), kIdentifierSize);
  memcpy(endpoint_descriptor_to_populate.peer_node_name,
         endpoint_being_sent->peer_address.node_name.c_str(), kIdentifierSize);
  memcpy(endpoint_descriptor_to_populate.peer_endpoint_name,
         endpoint_being_sent->peer_address.endpoint_name.c_str(),
         kIdentifierSize);
  printf("endpoint_descriptor_to_populate.endpoint_name: %s\n",
         endpoint_descriptor_to_populate.endpoint_name);
  printf("endpoint_descriptor_to_populate.cross_node_endpoint_name: %s\n",
         endpoint_descriptor_to_populate.cross_node_endpoint_name);
  printf("endpoint_descriptor_to_populate.peer_node_name: %s\n",
         endpoint_descriptor_to_populate.peer_node_name);
  printf("endpoint_descriptor_to_populate.peer_endpoint_name: %s\n",
         endpoint_descriptor_to_populate.peer_endpoint_name);
}

// static
MageHandle Core::RecoverExistingMageHandleFromEndpointDescriptor(
    const EndpointDescriptor& endpoint_descriptor) {
  printf(
      "Core::RecoverExistingMageHandleFromEndpointDescriptor(endpoint_"
      "descriptor)\n");
  std::string endpoint_name(
      endpoint_descriptor.endpoint_name,
      endpoint_descriptor.endpoint_name + kIdentifierSize);

  std::map<MageHandle, std::shared_ptr<Endpoint>>& handle_table =
      Core::Get()->handle_table_;
  // First see if the endpoint is already registered. If so, just early-return
  // the handle associated with it.
  for (std::map<MageHandle, std::shared_ptr<Endpoint>>::const_iterator it =
           handle_table.begin();
       it != handle_table.end(); it++) {
    if (it->second->name == endpoint_name) {
      return it->first;
    }
  }

  NOTREACHED();
}

// static
MageHandle Core::RecoverNewMageHandleFromEndpointDescriptor(
    const EndpointDescriptor& endpoint_descriptor) {
  printf(
      "Core::RecoverMageHandleFromEndpointDescriptor(endpoint_descriptor)\n");
  endpoint_descriptor.Print();

  std::string cross_node_endpoint_name(
      endpoint_descriptor.cross_node_endpoint_name,
      endpoint_descriptor.cross_node_endpoint_name + kIdentifierSize);

  // When we recover a new endpoint from a remote endpoint, the name we should
  // create it with is `cross_node_endpoint_name`, because this is the name
  // that the originator process generated for us so that it knows how to
  // target the remote endpoint.
  std::shared_ptr<Endpoint> local_endpoint(
      new Endpoint(/*name=*/cross_node_endpoint_name));
  local_endpoint->peer_address.node_name.assign(
      endpoint_descriptor.peer_node_name, kIdentifierSize);
  local_endpoint->peer_address.endpoint_name.assign(
      endpoint_descriptor.peer_endpoint_name, kIdentifierSize);
  MageHandle local_handle = Core::Get()->GetNextMageHandle();
  Core::Get()->RegisterLocalHandleAndEndpoint(local_handle,
                                              std::move(local_endpoint));
  return local_handle;
}

MageHandle Core::GetNextMageHandle() {
  return next_available_handle_++;
}

void Core::OnReceivedAcceptInvitation() {
  if (remote_has_accepted_invitation_callback_) {
    origin_task_runner_->PostTask(
        std::move(remote_has_accepted_invitation_callback_));
  }
}

void Core::OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint) {
  MageHandle local_handle = GetNextMageHandle();
  handle_table_.insert({local_handle, std::move(local_endpoint)});
  CHECK(finished_accepting_invitation_callback_);
  origin_task_runner_->PostTask(
      [=]() { finished_accepting_invitation_callback_(local_handle); });
}

void Core::RegisterLocalHandleAndEndpoint(
    MageHandle local_handle,
    std::shared_ptr<Endpoint> local_endpoint) {
  // First, we check that `local_handle` doesn't already point to an existing
  // endpoint.
  {
    auto endpoint_it = handle_table_.find(local_handle);
    CHECK_EQ(endpoint_it, handle_table_.end());
  }

  // Next, we check that `local_endpoint` doesn't already exist in this node.
  // TODO(domfarolino): Support the case where an endpoint travels back to a
  // node where it previously lived. This would require us relaxing this check,
  // but it's also a bit more work.
  {
    auto endpoint_it = node_->local_endpoints_.find(local_endpoint->name);
    CHECK_EQ(endpoint_it, node_->local_endpoints_.end());
  }

  // Finally, we can register the endpoint with `this` and `node_`.
  printf(
      "Core::RegisterLocalHandle registering local handle and endpoint with "
      "name: %s\n",
      local_endpoint->name.c_str());
  handle_table_.insert({local_handle, local_endpoint});
  node_->local_endpoints_.insert({local_endpoint->name, local_endpoint});
  printf("node_->local_endpoints_.size(): %lu\n",
         node_->local_endpoints_.size());
}

};  // namespace mage
