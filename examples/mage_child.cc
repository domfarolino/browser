#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "base/scheduling/task_loop_for_io.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"

int main(int argc, char** argv) {
  printf("-------- Child process --------\n");
  auto task_loop = base::TaskLoop::Create(base::ThreadType::IO);
  mage::Core::Init(static_pointer_cast<base::TaskLoopForIO>(task_loop).get());

  int fd = std::stoi(argv[1]);
  printf("File desc to read from is: %d\n", fd);

  mage::Core::AcceptInvitation(fd);

  task_loop->Run();
  return 0;
}
