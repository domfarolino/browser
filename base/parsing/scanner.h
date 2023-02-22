#ifndef BASE_PARSING_SCANNER_H_
#define BASE_PARSING_SCANNER_H_

#include <sstream>
#include <vector>

#include "base/parsing/document.h"
#include "base/parsing/token.h"

namespace base {

// |Scanner| accepts a Document instance and scans the text into Tokens, 
// which are serialized into a Token stream for parsing.
class Scanner {
  public:
    Scanner(Document *document);
    ~Scanner();

    // Get the next Token from the scanner
    std::shared_ptr<Token> next_token();

  private:
    Document *document_;
    std::vector<std::shared_ptr<Token>> token_vector_;
};
}; // namespace base

#endif // BASE_PARSING_SCANNER_H_
