#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <memory>

#include "base/callback.h"
#include "base/scheduling/task_loop_for_io.h"
#include "base/threading/thread_checker.h" // for CHECK_ON_THREAD().
#include "gtest/gtest.h"
#include "mage/bindings/receiver.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/test/magen/test.magen.h" // Generated.

namespace mage {

namespace {

void PRINT_THREAD() {
  if (base::GetIOThreadTaskLoop() == base::GetCurrentThreadTaskLoop()) {
    printf("[base::ThreadType::IO]\n");
  } else if (base::GetUIThreadTaskLoop() == base::GetCurrentThreadTaskLoop()) {
    printf("[base::ThreadType::UI]\n");
  }
}

// A concrete implementation of a mage test-only interface. This hooks in with
// the test fixture by invoking callbacks.
class TestInterfaceImpl : public magen::TestInterface {
 public:
  TestInterfaceImpl(MageHandle message_pipe, std::function<void()> quit_closure)
      : quit_closure_(std::move(quit_closure)) {
    receiver_.Bind(message_pipe, this);
  }

  void Method1(int in_int, double in_double, std::string in_string) {
    PRINT_THREAD();
    printf("TestInterfaceImpl::Method1\n");

    received_int = in_int;
    received_double = in_double;
    received_string = in_string;
    printf("[TestInterfaceImpl]: Quit() on closure we were given\n");
    quit_closure_();
  }

  void SendMoney(int in_amount, std::string in_currency) {
    PRINT_THREAD();
    printf("TestInterfaceImpl::SendMoney\n");

    received_amount = in_amount;
    received_currency = in_currency;
    printf("[TestInterfaceImpl]: Quit() on closure we were given\n");
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
  std::function<void()> quit_closure_;
};

enum class MageTestProcessType {
  kChildIsAccepterAndRemote,
  kChildIsInviterAndRemote,
  kInviterAsRemoteBlockOnAcceptance,
  kNone,
};

// TODO(domfarolino): It appears that theses all need the `./bazel-bin/` prefix
// to run correctly locally, but this breaks the GH workflow. Look into this.
static const char kChildAcceptorAndRemoteBinary[] = "./bazel-bin/mage/test/invitee_as_remote";
static const char kChildInviterAndRemoteBinary[] = "./bazel-bin/mage/test/inviter_as_remote";
static const char kInviterAsRemoteBlockOnAcceptancePath[] = "./bazel-bin/mage/test/inviter_as_remote_block_on_acceptance";

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
      char cwd[1024];
      getcwd(cwd, sizeof(cwd));
      printf("getcwd(): %s\n\n\n", cwd);

      switch (type_) {
        case MageTestProcessType::kChildIsAccepterAndRemote:
          rv = execl(kChildAcceptorAndRemoteBinary, "--mage-socket=", fd_as_string.c_str(), NULL);
          ASSERT_EQ(rv, 0);
          break;
        case MageTestProcessType::kChildIsInviterAndRemote:
          rv = execl(kChildInviterAndRemoteBinary, "--mage-socket=", fd_as_string.c_str(), NULL);
          ASSERT_EQ(rv, 0);
          break;
        case MageTestProcessType::kInviterAsRemoteBlockOnAcceptance:
          rv = execl(kInviterAsRemoteBlockOnAcceptancePath, "--mage-socket=", fd_as_string.c_str(), NULL);
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
    // TODO(domfarolino): Why do we need this?
    io_thread.GetTaskRunner()->PostTask(main_thread->QuitClosure());
    main_thread->Run();

    mage::Core::Init();
    EXPECT_TRUE(mage::Core::Get());
  }

  void TearDown() override {
    mage::Core::ShutdownCleanly();
    io_thread.StopWhenIdle(); // Blocks.
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
  mage::Core::AcceptInvitation(launcher->GetLocalFd(),
                               [](MageHandle) {
                                 NOTREACHED();
                               });

  // Invitation is asynchronous, so until we receive and formally accept the
  // information, there is no impact on our mage state.
  EXPECT_EQ(CoreHandleTable().size(), 0);
  EXPECT_EQ(NodeLocalEndpoints().size(), 0);
}

// In this test, the parent process is the inviter and a mage::Receiver.
TEST_F(MageTest, ParentIsInviterAndReceiver) {
  launcher->Launch(MageTestProcessType::kChildIsAccepterAndRemote);

  MageHandle message_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  // TODO(domfarolino): We have to use `std::bind()` here and `std::function`
  // the bound functor, because the test interface expects to be able to call
  // the quit closure multiple times. Migrate this to `base::RepeatingClosure`
  // when something like it exists.
  std::unique_ptr<TestInterfaceImpl> impl(
    new TestInterfaceImpl(message_pipe, std::bind(&base::TaskLoop::Quit, main_thread.get())));
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

// In this test, the parent process is the invitee and a mage::Receiver.
TEST_F(MageTest, ParentIsAcceptorAndReceiver) {
  launcher->Launch(MageTestProcessType::kChildIsInviterAndRemote);

  mage::Core::AcceptInvitation(launcher->GetLocalFd(), std::bind([&](MageHandle message_pipe){
    // TODO(domfarolino): This isn't right. I think this should be the UI thread.
    CHECK_ON_THREAD(base::ThreadType::IO);
    printf("PARENT PROCESS: AcceptInvitation callback is where we are\n");
    std::unique_ptr<TestInterfaceImpl> impl(
      new TestInterfaceImpl(message_pipe, std::bind(&base::TaskLoop::Quit, base::GetIOThreadTaskLoop().get())));

    // Let the message come in from the remote inviter.
    PRINT_THREAD();
    printf(" --> AcceptInvitation() about to run IO loop\n");
    base::GetCurrentThreadTaskLoop()->Run();

    // Once the mage method is invoked, the task loop will quit the above Run()
    // and we can check the results.
    printf(" --> AcceptInvitation() about to check results\n");
    EXPECT_EQ(impl->received_int, 1);
    EXPECT_EQ(impl->received_double, .5);
    EXPECT_EQ(impl->received_string, "message");

    // Let another message come in from the remote inviter.
    PRINT_THREAD();
    printf(" --> AcceptInvitation() about to run IO loop\n");
    base::GetCurrentThreadTaskLoop()->Run();
    EXPECT_EQ(impl->received_amount, 1000);
    EXPECT_EQ(impl->received_currency, "JPY");
    base::GetUIThreadTaskLoop()->Quit();
  }, std::placeholders::_1));

  // This will run the loop until we get the accept invitation. Then the above
  // lambda invokes, and the test continues in there.
  main_thread->Run();
}

/*
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
