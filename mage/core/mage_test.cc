#include <fcntl.h>
#include <signal.h>
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
#include "mage/test/magen/callback_interface.magen.h" // Generated.
#include "mage/test/magen/first_interface.magen.h" // Generated.
#include "mage/test/magen/second_interface.magen.h" // Generated.
#include "mage/test/magen/third_interface.magen.h" // Generated.
#include "mage/test/magen/fourth_interface.magen.h" // Generated.
#include "mage/test/magen/test.magen.h" // Generated.

namespace mage {

namespace {

void PRINT_THREAD() {
  if (base::GetIOThreadTaskLoop() == base::GetCurrentThreadTaskLoop()) {
    printf("[ThreadType::IO]\n");
  } else if (base::GetUIThreadTaskLoop() == base::GetCurrentThreadTaskLoop()) {
    printf("[ThreadType::UI]\n");
  }
}

// A concrete implementation of a mage test-only interface. This hooks in with
// the test fixture by invoking callbacks.
class TestInterfaceImpl : public magen::TestInterface {
 public:
  TestInterfaceImpl(MageHandle message_pipe) {
    receiver_.Bind(message_pipe, this);
  }

  void Method1(int in_int, double in_double, std::string in_string) {
    PRINT_THREAD();
    has_called_method1 = true;
    printf("TestInterfaceImpl::Method1\n");

    received_int = in_int;
    received_double = in_double;
    received_string = in_string;
    printf("[TestInterfaceImpl]: Quit() on closure we were given\n");
    base::GetCurrentThreadTaskLoop()->Quit();
  }

  void SendMoney(int in_amount, std::string in_currency) {
    PRINT_THREAD();
    has_called_send_money = true;
    printf("TestInterfaceImpl::SendMoney\n");

    received_amount = in_amount;
    received_currency = in_currency;
    printf("[TestInterfaceImpl]: Quit() on closure we were given\n");
    base::GetCurrentThreadTaskLoop()->Quit();
  }

  bool has_called_method1 = false;
  bool has_called_send_money = false;

  // Set by |Method1()| above.
  int received_int = 0;
  double received_double = 0.0;
  std::string received_string;

  // Set by |Method2()| above.
  int received_amount = 0;
  std::string received_currency;

 private:
  mage::Receiver<magen::TestInterface> receiver_;
};

// A concrete implementation of a mage test-only interface. This interface
// is used for remote process to tell the parent process when all operations are
// done.
class CallbackInterfaceImpl: public magen::CallbackInterface {
 public:
  CallbackInterfaceImpl(MageHandle message_pipe, std::function<void()> quit_closure)
      : quit_closure_(std::move(quit_closure)) {
    receiver_.Bind(message_pipe, this);
  }

  void NotifyDone() {
    quit_closure_();
  }

 private:
  mage::Receiver<magen::CallbackInterface> receiver_;

  // Can be called any number of times.
  std::function<void()> quit_closure_;
};

// The path for test binaries differs depending on whether the test is running
// as a result of a direct testing binary run (i.e., `./bazel-bin/[...]`) or the
// `bazel test` command. When `bazel test` is run, the `BAZEL_TEST` preprocessor
// directive is defined which helps us work out the right prefix.
#ifdef BAZEL_TEST
#define TEST_BINARY_PREFIX ""
#else
#define TEST_BINARY_PREFIX "./bazel-bin/"
#endif

#define RESOLVE_BINARY_PATH(x) TEST_BINARY_PREFIX #x

static const char kChildAcceptorAndRemote[] =
    RESOLVE_BINARY_PATH(mage/test/invitee_as_remote);
static const char kChildInviterAndRemote[] =
    RESOLVE_BINARY_PATH(mage/test/inviter_as_remote);
static const char kInviterAsRemoteBlockOnAcceptance[] =
    RESOLVE_BINARY_PATH(mage/test/inviter_as_remote_block_on_acceptance);
static const char kChildReceiveHandle[] =
    RESOLVE_BINARY_PATH(mage/test/child_as_receiver_and_callback);
static const char kParentReceiveHandle[] =
    RESOLVE_BINARY_PATH(mage/test/child_as_handle_sender);
static const char kChildSendsAcceptInvitationPipeToParent[] =
    RESOLVE_BINARY_PATH(mage/test/child_sends_accept_invitation_pipe_to_parent);
static const char kChildSendsSendInvitationPipeToParent[] =
    RESOLVE_BINARY_PATH(mage/test/child_sends_send_invitation_pipe_to_parent);
static const char kChildSendsTwoPipesToParent[] =
    RESOLVE_BINARY_PATH(mage/test/child_sends_two_pipes_to_parent);

class ProcessLauncher {
 public:
  ProcessLauncher() {
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_), 0);
    EXPECT_EQ(fcntl(fds_[0], F_SETFL, O_NONBLOCK), 0);
    EXPECT_EQ(fcntl(fds_[1], F_SETFL, O_NONBLOCK), 0);
  }
  ~ProcessLauncher() {
    EXPECT_EQ(close(fds_[0]), 0);
    EXPECT_EQ(close(fds_[1]), 0);
    if (child_pid_ != 0)
      EXPECT_EQ(kill(child_pid_, SIGKILL), 0);
  }

  void Launch(const char child_binary[]) {
    std::string fd_as_string = std::to_string(GetRemoteFd());
    pid_t rv = fork();
    if (rv != 0) { // Parent process.
      child_pid_ = rv;
      return;
    }

    // Child process.
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("getcwd(): %s\n\n\n", cwd);
    printf("Running binary: %s", child_binary);

    rv = execl(child_binary, "--mage-socket=", fd_as_string.c_str(), NULL);
    ASSERT_EQ(rv, 0);
  }

  int GetLocalFd() {
    return fds_[0];
  }

  int GetRemoteFd() {
    return fds_[1];
  }

 private:
  int fds_[2];
  // Set when we launch the child process.
  pid_t child_pid_ = 0;
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
  const auto& [local, remote] = Node().InitializeAndEntangleEndpoints();

  EXPECT_EQ(CoreHandleTable().size(), 0);
  EXPECT_EQ(NodeLocalEndpoints().size(), 0);

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
  EXPECT_EQ(CoreHandleTable().size(), 2);
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
  launcher->Launch(kChildAcceptorAndRemote);

  MageHandle message_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  TestInterfaceImpl impl(message_pipe);
  printf("[FROMUI] [Process: %d]: Run()\n", getpid());
  main_thread->Run();
  EXPECT_EQ(impl.received_int, 1);
  EXPECT_EQ(impl.received_double, .5);
  EXPECT_EQ(impl.received_string, "message");

  printf("[FROMUI] [Process: %d]: Run()\n", getpid());
  main_thread->Run();
  EXPECT_EQ(impl.received_amount, 1000);
  EXPECT_EQ(impl.received_currency, "JPY");
}

// In this test, the parent process is the invitee and a mage::Receiver.
TEST_F(MageTest, ParentIsAcceptorAndReceiver) {
  launcher->Launch(kChildInviterAndRemote);

  mage::Core::AcceptInvitation(launcher->GetLocalFd(),
                               std::bind([&](MageHandle message_pipe){
    EXPECT_NE(message_pipe, 0);
    EXPECT_EQ(CoreHandleTable().size(), 1);
    EXPECT_EQ(NodeLocalEndpoints().size(), 1);

    CHECK_ON_THREAD(base::ThreadType::UI);
    TestInterfaceImpl impl(message_pipe);

    // Let the message come in from the remote inviter.
    PRINT_THREAD();
    base::GetCurrentThreadTaskLoop()->Run();

    // Once the mage method is invoked, the task loop will quit the above Run()
    // and we can check the results.
    EXPECT_EQ(impl.received_int, 1);
    EXPECT_EQ(impl.received_double, .5);
    EXPECT_EQ(impl.received_string, "message");

    // Let another message come in from the remote inviter.
    PRINT_THREAD();
    base::GetCurrentThreadTaskLoop()->Run();
    EXPECT_EQ(impl.received_amount, 1000);
    EXPECT_EQ(impl.received_currency, "JPY");

    base::GetCurrentThreadTaskLoop()->Quit();
  }, std::placeholders::_1));

  // This will run the loop until we get the accept invitation. Then the above
  // lambda invokes, and the test continues in there.
  main_thread->Run();
}

// This test is the exact same as above, with the exception that the remote
// process we spawn (which sends the invitation and all of the messages that we
// receive) doesn't use its mage::Remote to talk to us until it receives our
// invitation acceptance. This is a subtle use-case to test because there is an
// internal difference with how mage messages are queued vs immediately sent
// depending on whether or not the mage::Remote is used before or after it
// learns that we accepted the invitation. This should be completely opaque to
// the user, which is why we have to test it.
TEST_F(MageTest, ParentIsAcceptorAndReceiverButChildBlocksOnAcceptance) {
  launcher->Launch(kInviterAsRemoteBlockOnAcceptance);

  mage::Core::AcceptInvitation(launcher->GetLocalFd(),
                               std::bind([&](MageHandle message_pipe){
    EXPECT_NE(message_pipe, 0);
    EXPECT_EQ(CoreHandleTable().size(), 1);
    EXPECT_EQ(NodeLocalEndpoints().size(), 1);

    CHECK_ON_THREAD(base::ThreadType::UI);
    TestInterfaceImpl impl(message_pipe);

    // Let the message come in from the remote inviter.
    base::GetCurrentThreadTaskLoop()->Run();

    // Once the mage method is invoked, the task loop will quit the above Run()
    // and we can check the results.
    EXPECT_EQ(impl.received_int, 1);
    EXPECT_EQ(impl.received_double, .5);
    EXPECT_EQ(impl.received_string, "message");

    // Let another message come in from the remote inviter.
    base::GetCurrentThreadTaskLoop()->Run();
    EXPECT_EQ(impl.received_amount, 1000);
    EXPECT_EQ(impl.received_currency, "JPY");

    base::GetCurrentThreadTaskLoop()->Quit();
  }, std::placeholders::_1));

  // This will run the loop until we get the accept invitation. Then the above
  // lambda invokes, and the test continues in there.
  main_thread->Run();
}

// The next three tests exercise the scenario where we send an invitation to
// another process, and send handle-bearing messages to the invited process. We
// expect messages that were queued on the handle being sent to get delivered to
// the remote process. There are five different ways to test this scenario, each
// varying in the timing of various events. We test all of them to protect
// against fragile implementation regressions.
//
// 01:
//   1.) Send invitation (pipe used for FirstInterface)
//   2.) Create message pipes for SecondInterface and callback
//   3.) Send one of SecondInterface's handles to other process via FirstInterface
//   4.) Send messages to SecondInterface and assert everything was received
//   5.) Send more messages and assert they are received
//
// 02:
//   1.) Send invitation (pipe used for FirstInterface)
//   2.) Create message pipes for SecondInterface and callback
//   3.) Send messages to SecondInterface
//   4.) Send one of SecondInterface's handles to other process via
//       FirstInterface and assert everything was received
//   5.) Send more messages and assert they are received
//
// 03 (Not tested; too similar):
//   1.) Create message pipes for SecondInterface and callback
//   2.) Send invitation (pipe used for FirstInterface)
//   3.) Send messages to SecondInterface
//   4.) Send one of SecondInterface's handles to other process via
//       FirstInterface and assert everything was received
//
// 04 (Not tested; too similar):
//   1.) Create message pipes for SecondInterface and callback
//   2.) Send invitation (pipe used for FirstInterface)
//   3.) Send one of SecondInterface's handles to other process via FirstInterface
//   4.) Send messages to SecondInterface and assert everything was received
//
// 05:
//   1.) Create message pipes for SecondInterface and callback
//   2.) Send messages to SecondInterface
//   3.) Send invitation (pipe used for FirstInterface)
//   4.) Send one of SecondInterface's handles to other process via
//       FirstInterface and assert everything was received
TEST_F(MageTest, SendHandleAndQueuedMessageOverInitialPipe_01) {
  launcher->Launch(kChildReceiveHandle);

  // 1.) Send invitation (pipe used for FirstInterface)
  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  EXPECT_EQ(CoreHandleTable().size(), 2);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  // 2.) Create message pipes for SecondInterface and callback
  std::vector<mage::MageHandle> second_handles = mage::Core::CreateMessagePipes();
  std::vector<mage::MageHandle> callback_handles = mage::Core::CreateMessagePipes();

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // 3.) Send one of SecondInterface's handles to other process via
  //     FirstInterface
  mage::Remote<magen::FirstInterface> remote;
  remote.Bind(invitation_pipe);
  remote->SendString("Message for FirstInterface");
  remote->SendHandles(second_handles[1], callback_handles[1]);

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // 4.) Send messages to SecondInterface and assert everything was received
  mage::Remote<magen::SecondInterface> second_remote;
  second_remote.Bind(second_handles[0]);
  second_remote->SendStringAndNotifyDoneViaCallback("Message for SecondInterface");

  std::unique_ptr<CallbackInterfaceImpl> impl(
    new CallbackInterfaceImpl(callback_handles[0],
        std::bind(&base::TaskLoop::Quit, main_thread.get())));

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // This will run the loop until we get the callback from the child saying
  // everything went through OK.
  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // 5.) Send more messages and assert they are received
  second_remote->NotifyDoneViaCallback();
  main_thread->Run();
}
TEST_F(MageTest, SendHandleAndQueuedMessageOverInitialPipe_02) {
  launcher->Launch(kChildReceiveHandle);

  // 1.) Send invitation (pipe used for FirstInterface)
  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  EXPECT_EQ(CoreHandleTable().size(), 2);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  // 2.) Create message pipes for SecondInterface and callback
  std::vector<mage::MageHandle> second_handles = mage::Core::CreateMessagePipes();
  std::vector<mage::MageHandle> callback_handles = mage::Core::CreateMessagePipes();

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // 3.) Send messages to SecondInterface
  mage::Remote<magen::SecondInterface> second_remote;
  second_remote.Bind(second_handles[0]);
  second_remote->SendStringAndNotifyDoneViaCallback("Message for SecondInterface");

  // 4.) Send one of SecondInterface's handles to other process via
  //     FirstInterface and assert everything was received
  mage::Remote<magen::FirstInterface> remote;
  remote.Bind(invitation_pipe);
  remote->SendString("Message for FirstInterface");
  remote->SendHandles(second_handles[1], callback_handles[1]);

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  std::unique_ptr<CallbackInterfaceImpl> impl(
    new CallbackInterfaceImpl(callback_handles[0],
        std::bind(&base::TaskLoop::Quit, main_thread.get())));

  // This will run the loop until we get the callback from the child saying
  // everything went through OK.
  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // 5.) Send more messages and assert they are received
  second_remote->NotifyDoneViaCallback();
  main_thread->Run();
}
TEST_F(MageTest, SendHandleAndQueuedMessageOverInitialPipe_05) {
  launcher->Launch(kChildReceiveHandle);

  // 1.) Create message pipes for SecondInterface and callback
  std::vector<mage::MageHandle> second_handles = mage::Core::CreateMessagePipes();
  std::vector<mage::MageHandle> callback_handles = mage::Core::CreateMessagePipes();

  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  // 2.) Send messages to SecondInterface
  mage::Remote<magen::SecondInterface> second_remote;
  second_remote.Bind(second_handles[0]);
  second_remote->SendStringAndNotifyDoneViaCallback("Message for SecondInterface");

  // 3.) Send invitation (pipe used for FirstInterface)
  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // 4.) Send one of SecondInterface's handles to other process via
  //     FirstInterface and assert everything was received
  mage::Remote<magen::FirstInterface> remote;
  remote.Bind(invitation_pipe);
  remote->SendString("Message for FirstInterface");
  remote->SendHandles(second_handles[1], callback_handles[1]);

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  std::unique_ptr<CallbackInterfaceImpl> impl(
    new CallbackInterfaceImpl(callback_handles[0],
        std::bind(&base::TaskLoop::Quit, main_thread.get())));

  // This will run the loop until we get the callback from the child saying
  // everything went through OK.
  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // 5.) Send more messages and assert they are received
  second_remote->NotifyDoneViaCallback();
  main_thread->Run();
}

// This test is very similar to the above although it builds off of it. The
// above test queues messages two levels deep, one of which is endpoint-baring,
// over the initial pipe derived from an invitation. This test does the same
// thing, but the queueing is not done on the initial invitation pipe, but on
// one created after the entire invitation ceremony is completed. This is
// because during development, there was a time where we treated message
// queueing differently depending on whether messages were queued on the
// invitation pipe or a generic pipe made arbitrarily later. This test shows
// that the two paths have been unified.
TEST_F(MageTest, SendHandleAndQueuedMessageOverArbitraryPipe) {
  launcher->Launch(kChildReceiveHandle);

  // 1.) Send invitation (pipe used for FirstInterface)
  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  // 2.) Create message pipes for SecondInterface and callback
  std::vector<mage::MageHandle> second_handles = mage::Core::CreateMessagePipes();
  std::vector<mage::MageHandle> callback_handles = mage::Core::CreateMessagePipes();

  // 3.) Send one of SecondInterface's handles to other process via
  //     FirstInterface and assert everything was received
  mage::Remote<magen::FirstInterface> remote;
  remote.Bind(invitation_pipe);
  remote->SendString("Message for FirstInterface");
  remote->SendHandles(second_handles[1], callback_handles[1]);

  // 4.) Send messages to SecondInterface and assert everything was received
  mage::Remote<magen::SecondInterface> second_remote;
  second_remote.Bind(second_handles[0]);
  second_remote->SendStringAndNotifyDoneViaCallback("Message for SecondInterface");

  std::unique_ptr<CallbackInterfaceImpl> impl(
    new CallbackInterfaceImpl(callback_handles[0],
        std::bind(&base::TaskLoop::Quit, main_thread.get())));

  // This will run the loop until we get the callback from the child saying
  // everything went through OK.
  main_thread->Run();

  // This is where the real meat of this test starts. We:
  //   1.) Create pipes for ThirdInterface
  //   2.) Bind a remote to ThirdInterface
  std::vector<mage::MageHandle> third_interface_handles = mage::Core::CreateMessagePipes();
  mage::Remote<magen::ThirdInterface> third_remote;
  third_remote.Bind(third_interface_handles[0]);

  //   3.) Create pipes for FourthInterface
  //   4.) Bind a remote to FourthInterface
  std::vector<mage::MageHandle> fourth_interface_handles = mage::Core::CreateMessagePipes();
  mage::Remote<magen::FourthInterface> fourth_remote;
  fourth_remote.Bind(fourth_interface_handles[0]);

  // Start queueing local messages on ThirdInterface and FourthInterface.
  third_remote->SendReceiverForFourthInterface(fourth_interface_handles[1]);
  fourth_remote->SendStringAndNotifyDoneViaCallback("Message for FourthInterface");

  // At this point we've queued a message on `third_remote` that sends a handle
  // over for `FourthInterface` and we've queued a message on `fourth_remote`.
  // This gives us two-levels deep of local queueing, which should be completely
  // flushed once we finally send the ThirdInterface receiver over to the child
  // process below.
  second_remote->SendReceiverForThirdInterface(third_interface_handles[1]);
  main_thread->Run();

  fourth_remote->NotifyDoneViaCallback();
  main_thread->Run();
}

// This implementation is only used for the `ReceiveHandleFromRemoteNode` test
// below.
class FirstInterfaceImplDummy final : public magen::FirstInterface {
 public:
  FirstInterfaceImplDummy(MageHandle handle) {
    receiver_.Bind(handle, this);
  }

  void SendString(std::string msg) override { NOTREACHED(); }
  void SendSecondInterfaceReceiver(MageHandle receiver) override {
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  void SendHandles(MageHandle, MageHandle) override { NOTREACHED(); }

 private:
  mage::Receiver<magen::FirstInterface> receiver_;
};
// This test shows that when a node receives a message with a single handle, we
// register two backing endpoints with `Core`.
TEST_F(MageTest, ReceiveHandleFromRemoteNode) {
  launcher->Launch(kParentReceiveHandle);

  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  FirstInterfaceImplDummy dummy(invitation_pipe);

  EXPECT_EQ(CoreHandleTable().size(), 2);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  main_thread->Run();

  // Once the message with a handle is received, we'll quit the loop and assert
  // that only one endpoint is registered with `Core`.
  EXPECT_EQ(CoreHandleTable().size(), 3);
  EXPECT_EQ(NodeLocalEndpoints().size(), 3);
}

class ChildPassInvitationPipeBackToParentMageHandler :
    public magen::FirstInterface, public magen::SecondInterface {
 public:
  ChildPassInvitationPipeBackToParentMageHandler(MageHandle receiver) {
    first_receiver_.Bind(receiver, this);
  }

  // FirstInterface overrides.
  void SendString(std::string) override {
    printf("ChildPassInvitationPipeBackToParentMageHandler::SendString() being called yessssss\n");
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  void SendSecondInterfaceReceiver(MageHandle receiver) override {
    second_receiver_.Bind(receiver, this);
  }
  void SendHandles(MageHandle, MageHandle) override { NOTREACHED(); }

  // SecondInterface overrides.
  // This is where the child's remote for `FirstInterface` will come in. We'll
  // save it for later use by the test.
  void SendReceiverForThirdInterface(MageHandle remote) override {
    remote_to_first_interface_ = remote;
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  void SendStringAndNotifyDoneViaCallback(std::string) override { NOTREACHED(); }
  void NotifyDoneViaCallback() override { NOTREACHED(); }

  MageHandle GetFirstInterfaceHandle() {
    // TODO(domfarolino): Can we check this against something more standard than
    // just 0 literal?
    CHECK_NE(remote_to_first_interface_, 0);
    return remote_to_first_interface_;
  }

 private:
  mage::Receiver<magen::FirstInterface> first_receiver_;
  mage::Receiver<magen::SecondInterface> second_receiver_;
  MageHandle remote_to_first_interface_;
};
// This test exercises weird but valid behavior: a child process takes the
// message pipe it gets from accepting an invitation from its parent, and sends
// it back to the parent (over a separate message pipe). Parent now has a
// remote/receiver and should be fully functional.
TEST_F(MageTest, ChildPassAcceptInvitationPipeBackToParent) {
  launcher->Launch(kChildSendsAcceptInvitationPipeToParent);

  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  ChildPassInvitationPipeBackToParentMageHandler handler(invitation_pipe);

  EXPECT_EQ(CoreHandleTable().size(), 2);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  MageHandle first_interface_remote_handle = handler.GetFirstInterfaceHandle();
  mage::Remote<magen::FirstInterface> remote;
  remote.Bind(first_interface_remote_handle);
  remote->SendString("This is my test");

  main_thread->Run();
}
// This test exercises weird but valid behavior: a child process takes the
// message pipe it gets from sending an invitation to its parent, and sends
// it back to the parent (over a separate message pipe). Parent now has a
// remote/receiver and should be fully functional.
TEST_F(MageTest, ChildPassSendInvitationPipeBackToParent) {
  launcher->Launch(kChildSendsSendInvitationPipeToParent);

  mage::Core::AcceptInvitation(launcher->GetLocalFd(),
                               std::bind([&](MageHandle message_pipe){
    // TODO(domfarolino): This seems to fuck with everything, figure this out.
    // sleep(1);
    ChildPassInvitationPipeBackToParentMageHandler handler(message_pipe);

    EXPECT_EQ(CoreHandleTable().size(), 1);
    EXPECT_EQ(NodeLocalEndpoints().size(), 1);

    main_thread->Run();

    EXPECT_EQ(NodeLocalEndpoints().size(), 3);
    EXPECT_EQ(CoreHandleTable().size(), 3);

    MageHandle first_interface_remote_handle = handler.GetFirstInterfaceHandle();
    mage::Remote<magen::FirstInterface> remote;
    remote.Bind(first_interface_remote_handle);
    remote->SendString("This is my test");

    main_thread->Run();
    main_thread->Quit();
  }, std::placeholders::_1));

  main_thread->Run();
}

// This class is only used for the `ChildPassRemoteAndReceiverToParent` test below.
class ChildPassTwoPipesToParent : public magen::FirstInterface,
                                  public magen::SecondInterface,
                                  public magen::ThirdInterface {
 public:
  ChildPassTwoPipesToParent(MageHandle receiver) {
    first_receiver_.Bind(receiver, this);
  }

  // FirstInterface overrides.
  void SendString(std::string) override { NOTREACHED(); }
  void SendSecondInterfaceReceiver(MageHandle receiver) override { NOTREACHED(); }
  void SendHandles(
      MageHandle remote_to_second_interface,
      MageHandle receiver_for_second_interface) override {
    remote_to_second_interface_ = remote_to_second_interface;
    second_receiver_.Bind(receiver_for_second_interface, this);
    base::GetCurrentThreadTaskLoop()->Quit();
  }

  // SecondInterface overrides.
  // Note that while the name here indicates that we're using the
  // `magen::Callback` interface to notify the parent that we've received the
  // message, we're actually going to abuse this API since it is bound to the
  // same process as the parent, and just quit the current loop.
  void SendStringAndNotifyDoneViaCallback(std::string msg) override {
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  // Duplicate of the `ThirdInterface` method.
  // void NotifyDoneViaCallback() override { NOTREACHED(); }
  void SendReceiverForThirdInterface(MageHandle receiver) override {
    third_receiver_.Bind(receiver, this);
    base::GetCurrentThreadTaskLoop()->Quit();
  }

  // ThirdInterface overrides.
  // Since we're same-process, we're not going to use the `magen::Callback`
  // interface. Instead we'll just quit the current task loop so that the test
  // resumes.
  void NotifyDoneViaCallback() override {
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  void SendReceiverForFourthInterface(MageHandle) override { NOTREACHED(); }

  MageHandle GetSecondInterfaceRemoteHandle() {
    // TODO(domfarolino): Can we check this against something more standard than
    // just 0 literal?
    CHECK_NE(remote_to_second_interface_, 0);
    return remote_to_second_interface_;
  }

 private:
  mage::Receiver<magen::FirstInterface> first_receiver_;
  mage::Receiver<magen::SecondInterface> second_receiver_;
  mage::Receiver<magen::ThirdInterface> third_receiver_;
  // The child process gives us remote and receiver handles for `magen::SecondInterface`.
  MageHandle remote_to_second_interface_;
};
// This test exercises behavior where a child process creates two entangled
// message pipes and sends both of them to the parent process. The two endpoints
// in the child process should be marked as proxying to each other. When the
// parent receives both pipes, it binds them to a remote/receiver pair and uses
// the remote to send a message that should end up on the same-process receiver.
TEST_F(MageTest, ChildPassRemoteAndReceiverToParent) {
  launcher->Launch(kChildSendsTwoPipesToParent);

  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  ChildPassTwoPipesToParent handler(invitation_pipe);

  EXPECT_EQ(CoreHandleTable().size(), 2);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  // Run until `handler` receives the `magen::SecondInterface` remote/receiver
  // pair. It will bind itself to the receiver, and we can get the remote via
  // `GetSecondInterfaceRemoteHandle()`.
  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  mage::Remote<magen::SecondInterface> remote;
  remote.Bind(handler.GetSecondInterfaceRemoteHandle());
  remote->SendStringAndNotifyDoneViaCallback("message");

  main_thread->Run();
}

// This test is very similar to the above, but slightly more complex. The two
// entangled endpoints get sent from child => parent, where they are bound to a
// remote/receiver pair. But the parent uses the remote to send an
// endpoint-baring message to the receiver, instead of a normal message. This
// exercises the case where a proxying endpoint not only has to edit the
// `target_endpoint` of the message it receives and forwards, but also has to
// set each endpoint that it received and processed into the proxying state as
// well.
TEST_F(MageTest, ChildPassRemoteAndReceiverToParentToSendEndpointBaringMessageOver) {
  launcher->Launch(kChildSendsTwoPipesToParent);

  MageHandle invitation_pipe =
    mage::Core::SendInvitationAndGetMessagePipe(
      launcher->GetLocalFd()
    );

  ChildPassTwoPipesToParent handler(invitation_pipe);

  EXPECT_EQ(CoreHandleTable().size(), 2);
  EXPECT_EQ(NodeLocalEndpoints().size(), 2);

  // Run until `handler` receives the `magen::SecondInterface` remote/receiver
  // pair. It will bind itself to the receiver, and we can get the remote via
  // `GetSecondInterfaceRemoteHandle()`.
  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  mage::Remote<magen::SecondInterface> remote;
  remote.Bind(handler.GetSecondInterfaceRemoteHandle());

  // Generate pipes for `magen::ThirdInterface` and send the receiver over
  // `SecondInterface`. It should make it back to `handler` which is the
  // `SecondInterface` implementation, and bind itself to the `ThirdInterface`
  // receiver too.
  std::vector<MageHandle> third_interface_handles = mage::Core::CreateMessagePipes();
  remote->SendReceiverForThirdInterface(third_interface_handles[1]);

  EXPECT_EQ(CoreHandleTable().size(), 6);
  EXPECT_EQ(NodeLocalEndpoints().size(), 6);

  // Run until `handle` receives the
  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 7);
  EXPECT_EQ(NodeLocalEndpoints().size(), 7);

  mage::Remote<magen::ThirdInterface> third_interface_remote;
  third_interface_remote.Bind(third_interface_handles[0]);
  third_interface_remote->NotifyDoneViaCallback();

  // Run until the notify-done message makes it to `handler`, in which case the
  // loop will quit and the test will end successfully.
  main_thread->Run();

  EXPECT_EQ(CoreHandleTable().size(), 7);
  EXPECT_EQ(NodeLocalEndpoints().size(), 7);
}

/////////////////////////////// IN-PROCESS TESTS ///////////////////////////////
TEST_F(MageTest, InProcessQueuedMessagesAfterReceiverBound) {
  std::vector<MageHandle> mage_handles = mage::Core::CreateMessagePipes();
  EXPECT_EQ(mage_handles.size(), 2);

  MageHandle local_handle = mage_handles[0],
             remote_handle = mage_handles[1];
  mage::Remote<magen::TestInterface> remote;
  remote.Bind(local_handle);
  TestInterfaceImpl impl(remote_handle);

  // At this point both the local and remote endpoints are bound. Invoke methods
  // on the remote, and verify that the receiver's implementation is not invoked
  // synchronously.
  remote->Method1(6000, .78, "some text");
  remote->SendMoney(5000, "USD");
  EXPECT_FALSE(impl.has_called_method1);
  EXPECT_FALSE(impl.has_called_send_money);

  // Verify that if a receiver is bound and multiple messages are queued on the
  // bound endpoint, each is delivered in their own task and they are not
  // delivered synchronously with respect to each other. `mage::Endpoint`
  // achieves this by posting a task to deliever each message to the receiving
  // delegate bound to the endpoint, which means in between each delivered
  // message, we return to the `TaskLoop` (that the receiving delegate was bound
  // on). In this case when the first message is delivered to the receiver, it
  // tells the `TaskLoop` to quit, so when we return to the `TaskLoop` before
  // posting the next message, we quit and continue running the assertions after
  // `Run()` below.
  main_thread->Run();
  EXPECT_TRUE(impl.has_called_method1);
  EXPECT_FALSE(impl.has_called_send_money);
  EXPECT_EQ(impl.received_int, 6000);
  EXPECT_EQ(impl.received_double, .78);
  EXPECT_EQ(impl.received_string, "some text");

  main_thread->Run();
  EXPECT_TRUE(impl.has_called_method1);
  EXPECT_TRUE(impl.has_called_send_money);
  EXPECT_EQ(impl.received_amount, 5000);
  EXPECT_EQ(impl.received_currency, "USD");
}
TEST_F(MageTest, InProcessQueuedMessagesBeforeReceiverBound) {
  std::vector<MageHandle> mage_handles = mage::Core::CreateMessagePipes();
  EXPECT_EQ(mage_handles.size(), 2);

  MageHandle local_handle = mage_handles[0],
             remote_handle = mage_handles[1];
  mage::Remote<magen::TestInterface> remote;
  remote.Bind(local_handle);
  remote->Method1(6000, .78, "some text");
  remote->SendMoney(5000, "USD");

  // At this point the local endpoint is bound and the two messages have been
  // sent. Bind the receiver and ensure that the messages are not delivered
  // synchronously.
  TestInterfaceImpl impl(remote_handle);
  EXPECT_FALSE(impl.has_called_method1);
  EXPECT_FALSE(impl.has_called_send_money);

  // Just like the `InProcessQueuedMessagesAfterReceiverBound` test above,
  // verify that multiple messages are not delivered to a receiver
  // synchronously.
  main_thread->Run();
  EXPECT_TRUE(impl.has_called_method1);
  EXPECT_FALSE(impl.has_called_send_money);
  EXPECT_EQ(impl.received_int, 6000);
  EXPECT_EQ(impl.received_double, .78);
  EXPECT_EQ(impl.received_string, "some text");

  main_thread->Run();
  EXPECT_TRUE(impl.has_called_send_money);
  EXPECT_EQ(impl.received_amount, 5000);
  EXPECT_EQ(impl.received_currency, "USD");
}

// These two interface implementations are specifically for the
// `OrderingNotPreservedBetweenPipes` test that follows them.
class SecondInterfaceImpl final : public magen::SecondInterface {
 public:
  // Called asynchronously by `FirstInterfaceImpl`.
  void Bind(MageHandle receiver) {
    receiver_.Bind(receiver, this);
  }

  // Note that even though this API indicates we're going to use the
  // `magen::Callback` interface to notify the parent process that we've
  // received the message, since this implementation is bound to the same
  // process as the parent, we're going to abuse the API a bit and just quit the
  // current task loop. That will allow the test to continue after we've
  // received this message, having the same intended effect.
  void SendStringAndNotifyDoneViaCallback(std::string msg) override {
    send_string_and_notify_done_called = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  void NotifyDoneViaCallback() override { NOTREACHED(); }
  // Not used for this test.
  void SendReceiverForThirdInterface(MageHandle) override { NOTREACHED(); }

  bool send_string_and_notify_done_called = false;

 private:
  mage::Receiver<magen::SecondInterface> receiver_;
};
class FirstInterfaceImpl final : public magen::FirstInterface {
 public:
  FirstInterfaceImpl(MageHandle handle, SecondInterfaceImpl& second_impl) :
      second_impl_(second_impl) {
    receiver_.Bind(handle, this);
  }

  void SendString(std::string msg) override {
    send_string_called = true;
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  void SendSecondInterfaceReceiver(MageHandle receiver) override {
    send_second_interface_receiver_called = true;
    second_impl_.Bind(receiver);
    base::GetCurrentThreadTaskLoop()->Quit();
  }
  void SendHandles(MageHandle, MageHandle) override { NOTREACHED(); }

  bool send_string_called = false;
  bool send_second_interface_receiver_called = false;

 private:
  mage::Receiver<magen::FirstInterface> receiver_;
  SecondInterfaceImpl& second_impl_;
};

// This test demonstrates that the ordering between message pipes cannot be
// guaranteed. In fact, the ordering of messages sent on two different message
// pipes (i.e., remote/receiver pairs) can almost always be guaranteed if both
// receivers are in the same process, but since there are cases where the
// ordering *is not* relied upon, the official guarantee is that ordering cannot
// be preserved between distinct message pipes. This test demonstrates that. We
// have two different interfaces `FirstInterface` and `SecondInterface`. We send
// the following messages:
//   1.) FirstInterface (carry SecondInterface)
//   2.) SecondInterface
//   3.) FirstInterface
//
// But we observe that the messages get delivered in the following order:
//  1.) FirstInterface (carry SecondInterface)
//  2.) FirstInterface
//  3.) SecondInterface
TEST_F(MageTest, OrderingNotPreservedBetweenPipes_SendBeforeReceiverBound) {
  std::vector<MageHandle> first_interface_handles = mage::Core::CreateMessagePipes();
  MageHandle first_remote_handle = first_interface_handles[0],
             first_receiver_handle = first_interface_handles[1];
  mage::Remote<magen::FirstInterface> first_remote;
  first_remote.Bind(first_remote_handle);

  std::vector<MageHandle> second_interface_handles = mage::Core::CreateMessagePipes();
  MageHandle second_remote_handle = second_interface_handles[0],
             second_receiver_handle = second_interface_handles[1];
  mage::Remote<magen::SecondInterface> second_remote;
  second_remote.Bind(second_remote_handle);

  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  // The actual message sending:
  first_remote->SendSecondInterfaceReceiver(second_receiver_handle);
  second_remote->SendStringAndNotifyDoneViaCallback("message");
  first_remote->SendString("Second message for FirstInterface");

  // The two backing implementations of our mage interfaces. `first_impl` gets
  // bound immediately, and `second_impl` gets bound asynchronously by `first_impl`.
  SecondInterfaceImpl second_impl;
  FirstInterfaceImpl first_impl(first_receiver_handle, second_impl);

  // None of the in-process sending actually changed how many handles or
  // endpoints were registered.
  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  // Observe the messages being received "out-of-order" compared to the order
  // they were sent in.
  main_thread->Run();
  EXPECT_TRUE(first_impl.send_second_interface_receiver_called);
  EXPECT_FALSE(first_impl.send_string_called);
  EXPECT_FALSE(second_impl.send_string_and_notify_done_called);

  main_thread->Run();
  EXPECT_TRUE(first_impl.send_second_interface_receiver_called);
  EXPECT_TRUE(first_impl.send_string_called);
  EXPECT_FALSE(second_impl.send_string_and_notify_done_called);

  main_thread->Run();
  EXPECT_TRUE(first_impl.send_second_interface_receiver_called);
  EXPECT_TRUE(first_impl.send_string_called);
  EXPECT_TRUE(second_impl.send_string_and_notify_done_called);
}
TEST_F(MageTest, OrderingNotPreservedBetweenPipes_SendAfterReceiverBound) {
  std::vector<MageHandle> first_interface_handles = mage::Core::CreateMessagePipes();
  MageHandle first_remote_handle = first_interface_handles[0],
             first_receiver_handle = first_interface_handles[1];
  mage::Remote<magen::FirstInterface> first_remote;
  first_remote.Bind(first_remote_handle);

  std::vector<MageHandle> second_interface_handles = mage::Core::CreateMessagePipes();
  MageHandle second_remote_handle = second_interface_handles[0],
             second_receiver_handle = second_interface_handles[1];
  mage::Remote<magen::SecondInterface> second_remote;
  second_remote.Bind(second_remote_handle);

  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  // The two backing implementations of our mage interfaces. `first_impl` gets
  // bound immediately, and `second_impl` gets bound asynchronously by `first_impl`.
  SecondInterfaceImpl second_impl;
  FirstInterfaceImpl first_impl(first_receiver_handle, second_impl);

  // This test is identical to the one above
  // (`OrderingNotPreservedBetweenPipes_SendBeforeReceiverBound`) except for the
  // fact that this sending code comes *after* the receiver set-up a couple
  // lines above.
  // The actual message sending:
  first_remote->SendSecondInterfaceReceiver(second_receiver_handle);
  second_remote->SendStringAndNotifyDoneViaCallback("message");
  first_remote->SendString("Second message for FirstInterface");

  // None of the in-process sending actually changed how many handles or
  // endpoints were registered.
  EXPECT_EQ(CoreHandleTable().size(), 4);
  EXPECT_EQ(NodeLocalEndpoints().size(), 4);

  // Observe the messages being received "out-of-order" compared to the order
  // they were sent in.
  main_thread->Run();
  EXPECT_TRUE(first_impl.send_second_interface_receiver_called);
  EXPECT_FALSE(first_impl.send_string_called);
  EXPECT_FALSE(second_impl.send_string_and_notify_done_called);

  main_thread->Run();
  EXPECT_TRUE(first_impl.send_second_interface_receiver_called);
  EXPECT_TRUE(first_impl.send_string_called);
  EXPECT_FALSE(second_impl.send_string_and_notify_done_called);

  main_thread->Run();
  EXPECT_TRUE(first_impl.send_second_interface_receiver_called);
  EXPECT_TRUE(first_impl.send_string_called);
  EXPECT_TRUE(second_impl.send_string_and_notify_done_called);
}

// A concrete implementation of a mage test-only interface that runs on a worker
// thread.
class TestInterfaceOnWorkerThread : public magen::TestInterface {
 public:
  TestInterfaceOnWorkerThread(MageHandle message_pipe,
                              std::function<void()> quit_closure)
      : quit_closure_(std::move(quit_closure)) {
    receiver_.Bind(message_pipe, this);
  }

  void Method1(int in_int, double in_double, std::string in_string) {
    CHECK(thread_checker_.CalledOnConstructedThread());
    has_called_method1 = true;
  }

  void SendMoney(int in_amount, std::string in_currency) {
    CHECK(thread_checker_.CalledOnConstructedThread());
    has_called_send_money = true;
    quit_closure_();
  }

  bool has_called_method1 = false;
  bool has_called_send_money = false;

 private:
  mage::Receiver<magen::TestInterface> receiver_;
  std::function<void()> quit_closure_;
  base::ThreadChecker thread_checker_;
};

TEST_F(MageTest, InProcessCrossThread) {
  base::Thread worker_thread(base::ThreadType::WORKER);
  worker_thread.Start();

  std::vector<MageHandle> mage_handles = mage::Core::CreateMessagePipes();
  EXPECT_EQ(mage_handles.size(), 2);

  MageHandle local_handle = mage_handles[0],
             remote_handle = mage_handles[1];
  mage::Remote<magen::TestInterface> remote;
  remote.Bind(local_handle);
  remote->Method1(6000, .78, "some text");
  remote->SendMoney(5000, "USD");

  // Bind the remote handle on the worker thread.
  std::unique_ptr<TestInterfaceOnWorkerThread> impl;
  worker_thread.GetTaskRunner()->PostTask([&](){
    impl = std::make_unique<TestInterfaceOnWorkerThread>(
        remote_handle,
        std::bind(&base::TaskLoop::Quit, base::GetUIThreadTaskLoop().get()));
  });

  // Run the main loop until we quit as a result of the last message being
  // delivered to `impl` which is bound on the worker thread. Note that
  // `TestInterfaceOnWorkerThread` only calls quit after the last message, not
  // each one. If it quit the main loop after each message, then we'd need a way
  // for the worker thread to halt between messages, so that it doesn't attempt
  // to quit the main loop twice in a row before we read even the first message.
  // That's racy.
  main_thread->Run();
  EXPECT_TRUE(impl->has_called_method1);
  EXPECT_TRUE(impl->has_called_send_money);
}

}; // namespace mage
