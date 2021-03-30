#include "gtest/gtest.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "base/callback.h"
#include "base/scheduling/task_loop_for_io.h"
#include "mage/bindings/receiver.h"
#include "mage/bindings/remote.h"
#include "mage/core/core.h"
#include "mage/core/handles.h"
#include "mage/test/magen/test.magen.h" // Generated.

namespace mage {

namespace {

class TestInterfaceImpl : public magen::TestInterface {
 public:
  TestInterfaceImpl(MageHandle message_pipe, base::Callback quit_closure) : quit_closure_(std::move(quit_closure)) {
    receiver_.Bind(message_pipe, this);
  }

  void Method1(int in_int, double in_double, std::string in_string) {
    received_int = in_int;
    received_double = in_double;
    received_string = in_string;
    quit_closure_();
  }

  // Set by |Method1()| above.
  int received_int = 0;
  double received_double = 0.0;
  std::string received_string;

 private:
  mage::Receiver<magen::TestInterface> receiver_;

  base::Callback quit_closure_;
};

enum class MageTestProcessType {
  kInviteeAsRemote,
  kInviterAsRemote,
  kNone,
};

static const char kInviteeAsRemotePath[] = "./mage/test/invitee_as_remote";
static const char kInviterAsRemotePath[] = "./mage/test/inviter_as_remote";

class ProcessLauncher {
 public:
  ProcessLauncher(MageTestProcessType type) : type_(type) {
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_), 0);
    EXPECT_EQ(fcntl(fds_[0], F_SETFL, O_NONBLOCK), 0);
    EXPECT_EQ(fcntl(fds_[1], F_SETFL, O_NONBLOCK), 0);
  }
  ~ProcessLauncher() {
    EXPECT_EQ(close(fds_[0]), 0);
    EXPECT_EQ(close(fds_[1]), 0);
    // Can we kill the child process too?
  }

  void Start() {
    std::string fd_as_string = std::to_string(GetRemoteFd());
    pid_t rv = fork();
    if (rv == 0) { // Child.
      switch (type_) {
        case MageTestProcessType::kInviteeAsRemote:
          rv = execl(kInviteeAsRemotePath, "--mage-socket=", fd_as_string.c_str());
          EXPECT_EQ(rv, 0);
          break;
        case MageTestProcessType::kInviterAsRemote:
          rv = execl(kInviterAsRemotePath, "--mage-socket=", fd_as_string.c_str());
          EXPECT_EQ(rv, 0);
          break;
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

class MageTestWrapper {
 public:
  std::map<MageHandle, std::shared_ptr<Endpoint>>& CoreHandleTable() {
    return mage::Core::Get()->handle_table_;
  }

  std::map<std::string, std::shared_ptr<Endpoint>>& NodeLocalEndpoints() {
    return mage::Core::Get()->node_->local_endpoints_;
  }

  mage::Node& Node() {
    return *mage::Core::Get()->node_.get();
  }
};

class MageTest : public testing::Test {
 public:
  void SetUp() override {
    mage::Core::Init();
    EXPECT_TRUE(mage::Core::Get());
  }

  void TearDown() override {
    // TODO(domfarolino): Maybe we should have a way to shutdown mage cleanly.
  }

 protected:
  MageTestWrapper wrapper;
};

TEST_F(MageTest, CoreInitStateUnitTest) {
  EXPECT_EQ(wrapper.CoreHandleTable().size(), 0);
  EXPECT_EQ(wrapper.NodeLocalEndpoints().size(), 0);
}

TEST_F(MageTest, InitializeAndEntangleEndpointsUnitTest) {
  std::shared_ptr<Endpoint> local(new Endpoint()), remote(new Endpoint());
  wrapper.Node().InitializeAndEntangleEndpoints(local, remote);

  EXPECT_EQ(wrapper.CoreHandleTable().size(), 0);
  EXPECT_EQ(wrapper.NodeLocalEndpoints().size(), 2);

  EXPECT_EQ(local->name.size(), 15);
  EXPECT_EQ(remote->name.size(), 15);
  EXPECT_NE(local->name, remote->name);

  // Both are pointing to the same node.
  EXPECT_EQ(local->peer_address.node_name, remote->peer_address.node_name);

  // Both are pointing at each other.
  EXPECT_EQ(local->peer_address.endpoint_name, remote->name);
  EXPECT_EQ(remote->peer_address.endpoint_name, local->name);
}

TEST_F(MageTest, SendInvitationUnitTest) {
  ProcessLauncher launcher(MageTestProcessType::kNone);
  std::shared_ptr<base::TaskLoop> task_loop_for_io =
    base::TaskLoop::Create(base::ThreadType::IO);

  MageHandle message_pipe =
    mage::Core::SendInvitationToTargetNodeAndGetMessagePipe(
      launcher.GetLocalFd()
    );

  EXPECT_NE(message_pipe, 0);
  EXPECT_EQ(wrapper.CoreHandleTable().size(), 1);
  EXPECT_EQ(wrapper.NodeLocalEndpoints().size(), 2);
}

TEST_F(MageTest, AcceptInvitationUnitTest) {
  ProcessLauncher launcher(MageTestProcessType::kNone);
  std::shared_ptr<base::TaskLoop> task_loop_for_io =
    base::TaskLoop::Create(base::ThreadType::IO);

  mage::Core::AcceptInvitation(launcher.GetLocalFd(), std::bind([](){}));

  // Invitation is asynchronous, so until we receive and formally accept the
  // information, there is no impact on our mage state.
  EXPECT_EQ(wrapper.CoreHandleTable().size(), 0);
  EXPECT_EQ(wrapper.NodeLocalEndpoints().size(), 0);
}

// In this test, the parent process is the inviter and a mage receiver.
TEST_F(MageTest, InviterAsReceiver) {
  ProcessLauncher launcher(MageTestProcessType::kInviteeAsRemote);
  launcher.Start();
  std::shared_ptr<base::TaskLoop> task_loop_for_io =
    base::TaskLoop::Create(base::ThreadType::IO);

  printf("*************** MageTest.InviterAsReceiver\n\n");
  fflush(stdout);
  MageHandle message_pipe =
    mage::Core::SendInvitationToTargetNodeAndGetMessagePipe(
      launcher.GetLocalFd()
    );

  std::unique_ptr<TestInterfaceImpl> impl(new TestInterfaceImpl(message_pipe, task_loop_for_io->QuitClosure()));
  task_loop_for_io->Run();
  EXPECT_EQ(impl->received_int, 1);
  EXPECT_EQ(impl->received_double, .5);
  EXPECT_EQ(impl->received_string, "message");
}

// In this test, the parent process is the inviter and a mage receiver.
TEST_F(MageTest, InviteeAsReceiver) {
  ProcessLauncher launcher(MageTestProcessType::kInviterAsRemote);
  launcher.Start();
  std::shared_ptr<base::TaskLoop> task_loop_for_io =
    base::TaskLoop::Create(base::ThreadType::IO);

  mage::Core::AcceptInvitation(launcher.GetLocalFd(), std::bind([&](MageHandle message_pipe){
    std::unique_ptr<TestInterfaceImpl> impl(new TestInterfaceImpl(message_pipe, task_loop_for_io->QuitClosure()));
    // Let the message come in from the remote inviter.
    task_loop_for_io->Run();
    EXPECT_EQ(impl->received_int, 1);
    EXPECT_EQ(impl->received_double, .5);
    EXPECT_EQ(impl->received_string, "message");
    task_loop_for_io->Quit();
  }, std::placeholders::_1));
  task_loop_for_io->Run();
}

}; // namespace mage
