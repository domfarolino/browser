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
    Token current_token_;
    Document document_;
    // AST

    bool run_parse(); // Should return DOM object
    void advance_token();
    Token next_token();
    bool check_lexeme(); // change this to token class
    void raise_parse_error();
};
}; // namespace base

#endif // BASE_PARSING_HTML_PARSER_H_
