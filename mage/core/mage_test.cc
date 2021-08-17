#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <memory>

#include "base/callback.h"
#include "base/scheduling/task_loop_for_io.h"
#include "gtest/gtest.h"
#include "mage/bindings/receiver.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/test/magen/test.magen.h" // Generated.

namespace mage {

namespace {

// A concrete implementation of a mage test-only interface. This hooks in with
// the test fixture by invoking callbacks.
class TestInterfaceImpl : public magen::TestInterface {
 public:
  TestInterfaceImpl(MageHandle message_pipe, base::Callback quit_closure) : quit_closure_(std::move(quit_closure)) {
    receiver_.Bind(message_pipe, this);
  }

  void PRINT_THREAD() {
    if (base::GetIOThreadTaskLoop() == base::GetCurrentThreadTaskLoop()) {
      printf("THREADTYPE::IO\n");
    } else if (base::GetUIThreadTaskLoop() == base::GetCurrentThreadTaskLoop()) {
      printf("THREADTYPE::UI\n");
    }
  }

  void Method1(int in_int, double in_double, std::string in_string) {
    PRINT_THREAD();
    printf("TestInterfaceImpl::Method1\n");

    received_int = in_int;
    received_double = in_double;
    received_string = in_string;
    printf("[FROMIO]: Quit()\n");
    quit_closure_();
  }

  void SendMoney(int in_amount, std::string in_currency) {
    PRINT_THREAD();
    printf("TestInterfaceImpl::SendMoney\n");

    received_amount = in_amount;
    received_currency = in_currency;
    printf("[FROMIO]: Quit()\n");
    quit_closure_();
  }

  // Set by |Method1()| above.
  int received_int = 0;
  double received_double = 0.0;
  std::string received_string;

  // Set by |Method2()| above.
  int received_amount = 0;
  std::string received_currency;

 private:
  mage::Receiver<magen::TestInterface> receiver_;

  // Can be called any number of times.
  base::Callback quit_closure_;
};

enum class MageTestProcessType {
  kInviteeAsRemote,
  kInviterAsRemote,
  kInviterAsRemoteBlockOnAcceptance,
  kNone,
};

static const char kInviteeAsRemotePath[] = "./bazel-bin/mage/test/invitee_as_remote";
static const char kInviterAsRemotePath[] = "./mage/test/inviter_as_remote";
static const char kInviterAsRemoteBlockOnAcceptancePath[] = "./mage/test/inviter_as_remote_block_on_acceptance";

class ProcessLauncher {
 public:
  ProcessLauncher() : type_(MageTestProcessType::kNone) {
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_), 0);
    EXPECT_EQ(fcntl(fds_[0], F_SETFL, O_NONBLOCK), 0);
    EXPECT_EQ(fcntl(fds_[1], F_SETFL, O_NONBLOCK), 0);
  }
  ~ProcessLauncher() {
    EXPECT_EQ(close(fds_[0]), 0);
    EXPECT_EQ(close(fds_[1]), 0);
    // Can we kill the child process too?
  }

  void Launch(MageTestProcessType type) {
    type_ = type;

    std::string fd_as_string = std::to_string(GetRemoteFd());
    pid_t rv = fork();
    if (rv == 0) { // Child.
      switch (type_) {
        case MageTestProcessType::kInviteeAsRemote:
          rv = execl(kInviteeAsRemotePath, "--mage-socket=", fd_as_string.c_str());
          EXPECT_EQ(rv, 0);
          printf("errono: %d\n", errno);

          char cwd[1024];
          getcwd(cwd, sizeof(cwd));
          printf("getcwd(): %s\n", cwd);

          break;
        case MageTestProcessType::kInviterAsRemote:
          rv = execl(kInviterAsRemotePath, "--mage-socket=", fd_as_string.c_str());
          EXPECT_EQ(rv, 0);
          break;
        case MageTestProcessType::kInviterAsRemoteBlockOnAcceptance:
          rv = execl(kInviterAsRemoteBlockOnAcceptancePath, "--mage-socket=", fd_as_string.c_str());
        case MageTestProcessType::kNone:
          NOTREACHED();
          break;
      }
    }
  }

  int GetLocalFd() {
    return fds_[0];
  }

  int GetRemoteFd() {
    return fds_[1];
  }

 private:
  int fds_[2];
  MageTestProcessType type_;
};

}; // namespace

class MageTest : public testing::Test {
 public:
  MageTest(): io_thread(base::ThreadType::IO) {}

  void SetUp() override {
    launcher = std::unique_ptr<ProcessLauncher>(new ProcessLauncher());
    main_thread = base::TaskLoop::Create(base::ThreadType::UI);
    io_thread.Start();
    io_thread.GetTaskRunner()->PostTask(main_thread->QuitClosure());
    main_thread->Run();

    mage::Core::Init();
    EXPECT_TRUE(mage::Core::Get());
  }

  void TearDown() override {
    mage::Core::ShutdownCleanly();
    // TODO(domfarolino): We should consider introducing
    // {TaskLoop,Thread}::{Quit,Stop}WhenIdle(), so we don't have to use this
    // manual post-task-and-wait technique to verify that the IO thread is
    // "finished".
    io_thread.GetTaskRunner()->PostTask(main_thread->QuitClosure());
    main_thread->Run(); // Waits for tasks to drain on the IO thread.
    io_thread.Stop();
    main_thread.reset();
    launcher.reset();
  }

 protected:
  std::map<MageHandle, std::shared_ptr<Endpoint>>& CoreHandleTable() {
    return mage::Core::Get()->handle_table_;
  }

  std::map<std::string, std::shared_ptr<Endpoint>>& NodeLocalEndpoints() {
    return mage::Core::Get()->node_->local_endpoints_;
  }

  mage::Node& Node() {
    return *mage::Core::Get()->node_.get();
  }

  std::unique_ptr<ProcessLauncher> launcher;
  std::shared_ptr<base::TaskLoop> main_thread;
  base::Thread io_thread;
};

TEST_F(MageTest, CoreInitStateUnitTest) {
  EXPECT_EQ(CoreHandleTable().size(), 0);
  EXPECT_EQ(NodeLocalEndpoints().size(), 0);
}

TEST_F(MageTest, InitializeAndEntangleEndpointsUnitTest) {
  std::shared_ptr<Endpoint> local(new Endpoint()), remote(new Endpoint());
  Node().InitializeAndEntangleEndpoints(local, remote);

  EXPECT_EQ(CoreHandleTable().size(), 0);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  EXPECT_EQ(local->name.size(), 15);
  EXPECT_EQ(remote->name.size(), 15);
  EXPECT_NE(local->name, remote->name);

  // Both endpoints address the same node name.
  EXPECT_EQ(local->peer_address.node_name, remote->peer_address.node_name);

  // Both endpoints address each other.
  EXPECT_EQ(local->peer_address.endpoint_name, remote->name);
  EXPECT_EQ(remote->peer_address.endpoint_name, local->name);
}

TEST_F(MageTest, SendInvitationUnitTest) {
  MageHandle message_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  EXPECT_NE(message_pipe, 0);
  EXPECT_EQ(CoreHandleTable().size(), 1);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  // Test that we can queue messages.
  mage::Remote<magen::TestInterface> remote;
  remote.Bind(message_pipe);
  remote->Method1(1, .4, "test");
}

TEST_F(MageTest, AcceptInvitationUnitTest) {
  mage::Core::AcceptInvitation(launcher->GetLocalFd(), std::bind([](){}));

  // Invitation is asynchronous, so until we receive and formally accept the
  // information, there is no impact on our mage state.
  EXPECT_EQ(CoreHandleTable().size(), 0);
  EXPECT_EQ(NodeLocalEndpoints().size(), 0);
}

// In this test, the parent process is the inviter and a mage::Receiver.
TEST_F(MageTest, InviterAsReceiver) {
  launcher->Launch(MageTestProcessType::kInviteeAsRemote);

  MageHandle message_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  std::unique_ptr<TestInterfaceImpl> impl(new TestInterfaceImpl(message_pipe, main_thread->QuitClosure()));
  printf("[FROMUI]: Run()\n");
  main_thread->Run();
  EXPECT_EQ(impl->received_int, 1);
  EXPECT_EQ(impl->received_double, .5);
  EXPECT_EQ(impl->received_string, "message");

  printf("[FROMUI]: Run()\n");
  main_thread->Run();
  EXPECT_EQ(impl->received_amount, 1000);
  EXPECT_EQ(impl->received_currency, "JPY");
}

/*
// In this test, the parent process is the invitee and a mage::Receiver.
TEST_F(MageTest, InviteeAsReceiver) {
  ProcessLauncher launcher(MageTestProcessType::kInviterAsRemote);
  launcher.Start();
  std::shared_ptr<base::TaskLoop> task_loop_for_io =
    base::TaskLoop::Create(base::ThreadType::IO);

  mage::Core::AcceptInvitation(launcher.GetLocalFd(), std::bind([&](MageHandle message_pipe){
    std::unique_ptr<TestInterfaceImpl> impl(new TestInterfaceImpl(message_pipe, task_loop_for_io->QuitClosure()));

    // Let the message come in from the remote inviter.
    task_loop_for_io->Run();

    // Once the mage method is invoked, the task loop will quit the above Run()
    // and we can check the results.
    EXPECT_EQ(impl->received_int, 1);
    EXPECT_EQ(impl->received_double, .5);
    EXPECT_EQ(impl->received_string, "message");

    // Let another message come in from the remote inviter.
    task_loop_for_io->Run();
    EXPECT_EQ(impl->received_amount, 1000);
    EXPECT_EQ(impl->received_currency, "JPY");

    task_loop_for_io->Quit();
  }, std::placeholders::_1));

  // This will run the loop until we get the accept invitation. Then the above
  // lambda invokes, and the test continues in there.
  task_loop_for_io->Run();
}

// This test is the exact same as above, with the exception that the remote
// process we spawn (which sends the invitation) doesn't use its mage::Remote to
// talk to us until it receives our invitation acceptance. This is a subtle use
// case to test because there is an internal difference with how mage messages
// are queued vs immediately sent depending on whether or not the mage::Remote
// is used before or after it learns that we accepted the invitation. This
// should be completely opaque to the user, which is why we have to test it.
TEST_F(MageTest, InviteeAsReceiverBlockOnAcceptance) {
  ProcessLauncher launcher(MageTestProcessType::kInviterAsRemoteBlockOnAcceptance);
  launcher.Start();
  std::shared_ptr<base::TaskLoop> task_loop_for_io =
    base::TaskLoop::Create(base::ThreadType::IO);

  mage::Core::AcceptInvitation(launcher.GetLocalFd(),
                               std::bind([&](MageHandle message_pipe){
    std::unique_ptr<TestInterfaceImpl> impl(
      new TestInterfaceImpl(message_pipe, task_loop_for_io->QuitClosure()));

    // Let the message come in from the remote inviter.
    task_loop_for_io->Run();

    // Once the mage method is invoked, the task loop will quit the above Run()
    // and we can check the results.
    EXPECT_EQ(impl->received_int, 1);
    EXPECT_EQ(impl->received_double, .5);
    EXPECT_EQ(impl->received_string, "message");

    // Let another message come in from the remote inviter.
    task_loop_for_io->Run();
    EXPECT_EQ(impl->received_amount, 1000);
    EXPECT_EQ(impl->received_currency, "JPY");

    task_loop_for_io->Quit();
  }, std::placeholders::_1));

  // This will run the loop until we get the accept invitation. Then the above
  // lambda invokes, and the test continues in there.
  task_loop_for_io->Run();
}

TEST_F(MageTest, InProcess) {
  std::shared_ptr<base::TaskLoop> task_loop_for_io =
    base::TaskLoop::Create(base::ThreadType::IO);

  std::vector<MageHandle> mage_handles = mage::Core::CreateMessagePipes();
  EXPECT_EQ(mage_handles.size(), 2);

  MageHandle local_handle = mage_handles[0], remote_handle = mage_handles[1];
  mage::Remote<magen::TestInterface> remote;
  std::unique_ptr<TestInterfaceImpl> impl(new TestInterfaceImpl(remote_handle, task_loop_for_io->QuitClosure()));

  remote->Method1(101, .78, "some text");
  remote->SendMoney(5000, "USD");
  task_loop_for_io->Run();
  EXPECT_EQ(impl->received_int, 1);
  EXPECT_EQ(impl->received_double, .5);
  EXPECT_EQ(impl->received_string, "message");

  // task_loop_for_io->Run();
  EXPECT_EQ(impl->received_amount, 1000);
  EXPECT_EQ(impl->received_currency, "JPY");
}

TEST_F(MageTest, InProcessQueuedMessagesDispatchSynchronously) {}
*/

}; // namespace mage

/*
  Spec
   - In process works
     - Test the local path
   - In process use remote before receiver bound

*/
