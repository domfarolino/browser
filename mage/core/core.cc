#include "mage/core/core.h"

#include <cstdlib>
#include <unistd.h>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/endpoint.h"
#include "mage/core/util.h"

namespace mage {

Core* g_core;

void Core::Init() {
  srand(getpid());

  g_core = new Core();
}

Core* Core::Get() {
  return g_core;
}

MageHandle Core::GetNextMageHandle() {
  return next_available_handle_++;
}

void Core::OnReceivedInvitation(std::shared_ptr<Endpoint> local_endpoint) {
  printf("Core::OnReceivedInvitation\n");
  MageHandle local_handle = GetNextMageHandle();
  handle_table_.insert({local_handle, std::move(local_endpoint)});
  CHECK(async_invitation_handler_);
  async_invitation_handler_(local_handle);
}

}; // namspace mage
