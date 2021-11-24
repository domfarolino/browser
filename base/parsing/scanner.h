#ifndef BASE_PARSING_SCANNER_H_
#define BASE_PARSING_SCANNER_H_

#include "base/parsing/document.h"
#include "base/parsing/token.h"

#include <stringstream>

namespace base {

// |Scanner| accepts a Document instance and scans the text into Tokens, which are serialized into a Token stream for parsing.
class Scanner {
  public:
    Scanner(Document *document);

    ~Scanner();

    // Get the next Token from the scanner
    Token* next_token();

  private:
    Document *document_;
    std::vector<Token> token_vector_;
};
}; // namespace base

#endif // BASE_PARSING_SCANNER_H_
