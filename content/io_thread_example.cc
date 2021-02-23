#include <assert.h>
#include <iostream>

#include "base/check.h"
#include "base/scheduling/task_loop.h"
#include "base/threading/thread.h"

void CalledOnIOThread() {
  printf("|CalledOnIOThread()| was invoked on the IO thread!!\n");
}

int main() {
  // Set up a task loop on the current thread. There is no need to use
  // base::Thread for this, since we don't want to spin up a new physical
  // thread.
  auto task_loop = base::TaskLoop::Create(base::ThreadType::WORKER);

  base::Thread io_thread(base::ThreadType::IO);
  io_thread.Start();

  printf("Main thread is posting a task to the IO thread\n");
  io_thread.GetTaskRunner()->PostTask(&CalledOnIOThread);
  io_thread.GetTaskRunner()->PostTask(&CalledOnIOThread);
  io_thread.GetTaskRunner()->PostTask(&CalledOnIOThread);

  task_loop->Run();
  return 0;
}
