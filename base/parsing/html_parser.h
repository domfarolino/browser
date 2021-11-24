#ifndef BASE_PARSING_HTML_PARSER_H_
#define BASE_PARSING_HTML_PARSER_H_

#include "base/parsing/document.h"
#include <stdio.h>
#include <string>

namespace base {

// |HTMLParser| is initialized with a Document instance, which is passed to
// the Scanner.  The Scanner tokenizes the document, and returns tokens to the
// Parser, which checks against the grammar.
//
// This class should return either an AST representing the parsed HTML document
// or a DOM object.
class HTMLParser {
  public:
    HTMLParser(Document *document);
    ~HTMLParser();

    bool run_parse(); // Should return DOM object

    void setDocument(Document *document);

  private:
    Token *current_token_;
    Scanner *scanner_;
    // AST

    void advance_token();
    bool check_lexeme(std::string lexeme); // change this to token class
    void raise_parse_error(std::string token_name);

    bool document_helper();
    bool html_open();
    bool head();
    bool head_open();
    bool head_close();
    bool html_close();
    bool head_resource();
    bool body();
    bool body_open();
    bool body_close();
    bool body_elements();
    bool div();
    bool div_open();
    bool div_close();
    bool img();
    bool text();
    bool attr_list();
    bool check_width();
    bool check_height();
};
}; // namespace base

#endif // BASE_PARSING_HTML_PARSER_H_
