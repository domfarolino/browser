#include "base/parsing/document.h"
#include "gtest/gtest.h"

namespace base {

TEST(Parsing, DocumentSetsRawText) {
  Document *document = new Document("<html></html>");

  EXPECT_TRUE(!document->getText().empty());

  std::string raw_text = "<html><body></body></html>";
  document->setText(raw_text);

  EXPECT_EQ(document->getText(), raw_text);
}

}; // namespace base
