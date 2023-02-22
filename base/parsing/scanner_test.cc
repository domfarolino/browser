#include "base/parsing/scanner.h"
#include "gtest/gtest.h"

namespace base {

TEST(Scanner, ScannerWithDocumentReturnsInitialToken) {
  Document *document = new Document("<html><body></body></html>");
  Scanner *scanner = new Scanner(document);

  EXPECT_EQ(scanner->next_token()->lexeme, "<html>");
}

}; // namespace base
