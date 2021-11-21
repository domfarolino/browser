#include "base/parsing/html_parser.h"
#include "gtest/gtest.h"

namespace base {

class HTMLParserTestBase : public testing::Test {
 public:
  void SetUp() {
    std::string text = "";
    Document *document = new Document(text);
    parser = new HTMLParser(document);
  }

  void TearDown() {

  }

  void modify_document_text(std::string text) {
    Document *document = new Document(text);
    parser->setDocument(document);
  }

  HTMLParser *parser;
};


TEST_F(HTMLParserTestBase, HTMLParserDocumentHasBody) {
  std::string text = "<html><head><body></body></head></html>";
  modify_document_text(text);
  bool parse_result = parser->run_parse();
  EXPECT_TRUE(parse_result);
}

}; // namespace base
