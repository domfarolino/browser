#include "gtest/gtest.h"

namespace base {

class HTMLParserTestBase : public testing::Test {
 public:
  void SetUp() {

  }

  void TearDown() {

  }
};

// void WriteMessages(int fd, std::vector<std::string> messages) {
//   WriteMessagesAndInvokeCallback(fd, messages, [](){});
// }

// TEST_F(TaskLoopForIOTestBase, BasicSocketReading) {
//   SetExpectedMessageCount(1);

//   std::unique_ptr<TestSocketReader> reader(new TestSocketReader(fds[0], *this));

//   std::vector<std::string> messages_to_write = {kFirstMessage};
//   base::SimpleThread simple_thread(WriteMessages, fds[1], messages_to_write);
//   task_loop_for_io->Run();
//   EXPECT_EQ(messages_read.size(), 1);
//   EXPECT_EQ(messages_read[0], kFirstMessage);
// }
}; // namespace base
