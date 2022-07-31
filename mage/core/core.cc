#include "mage/core/core.h"

#include <cstdlib>
#include <unistd.h>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/endpoint.h"
#include "mage/core/util.h"

namespace mage {

Core* g_core = nullptr;

void Core::Init() {
  srand(getpid());

  CHECK(!g_core);
  g_core = new Core();
}

void Core::ShutdownCleanly() {
  delete g_core;
  g_core = nullptr;
}

Core* Core::Get() {
  return g_core;
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
  origin_task_runner_->PostTask([=](){
    finished_accepting_invitation_callback_(local_handle);
  });
}

void Core::RegisterLocalHandleAndEndpoint(MageHandle local_handle, std::shared_ptr<Endpoint> local_endpoint) {
  // First, we check that `local_handle` doesn't already point to an existing endpoint.
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
  printf("Core::RegisterLocalHandle registering local handle and endpoint with name: %s\n", local_endpoint->name.c_str());
  handle_table_.insert({local_handle, local_endpoint});
  node_->local_endpoints_.insert({local_endpoint->name, local_endpoint});
  printf("node_->local_endpoints_.size(): %lu\n", node_->local_endpoints_.size());
}

}; // namspace mage
