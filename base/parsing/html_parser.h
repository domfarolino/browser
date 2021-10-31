#ifndef BASE_PARSING_HTML_PARSER_H_
#define BASE_PARSING_HTML_PARSER_H_

#include <stdio.h>

namespace base {
enum class TokenClass {

}

// |HTMLParser|  
class HTMLParser {
  public:
    HTMLParser(Document document) : document_(document);
    ~HTMLParser();

    class Scanner {

    }

    class Token {

    }

  private:
    Document document_;
    // AST
    bool run_parse(); // Should return DOM object
    void next_token();
    void check_token();
};
}; // namespace base

#endif // BASE_PARSING_HTML_PARSER_H_
