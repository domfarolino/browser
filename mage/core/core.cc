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
  if (remote_has_accepted_invitation_callback_)
    remote_has_accepted_invitation_callback_();
}

void Core::OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint) {
  MageHandle local_handle = GetNextMageHandle();
  handle_table_.insert({local_handle, std::move(local_endpoint)});
  CHECK(finished_accepting_invitation_callback_);
  finished_accepting_invitation_callback_(local_handle);
}

void Core::RegisterLocalHandle(MageHandle local_handle, std::shared_ptr<Endpoint> local_endpoint) {
  auto endpoint_it = handle_table_.find(local_handle);
  CHECK_EQ(endpoint_it, handle_table_.end());
  handle_table_.insert({local_handle, local_endpoint});
}

}; // namspace mage
