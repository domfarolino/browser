#include "base/parsing/html_parser.h"


namespace base {

bool img();
bool div();
bool text();
bool advance_token();

// Update the current token class member
void HTMLParser::advance_token() {
  current_token_ = scanner_->next_token();
}


// Check that the lexeme parsed matches
//  This function should be change to check the 
//  token class, checked against an enum
bool HTMLParser::check_lexeme(std::string lexeme) {
  return (current_token_->lexeme == lexeme);
}


void HTMLParser::raise_parse_error(std::string token_name) {
  std::cout << "Parse Error:  Expected token " << token_name << "\n\n";
}


// run_parse - starts the process of parsing the document
// 
bool HTMLParser::run_parse() {
  return document_helper();
}


void HTMLParser::setDocument(Document *document) {
  document_ = document;
}

// <document> ::= <html_open><head><body><html_close>
//
bool HTMLParser::document_helper() {
  bool result = false;

  if (html_open()) {
    // Should make these optional, instead of required
    if (head()) {
      if (body()) {
        if (html_close()) {
          result = true;
        } else {
          raise_parse_error("html_close");
        }
      } else {
        raise_parse_error("body");
      }
    }
  } else {
    // need a debug log level here
    raise_parse_error("html_open");
  }

  return result;
}

// <html_open> ::= "<html>"
//
bool HTMLParser::html_open() {
  advance_token();
  return check_lexeme("<html>");
}

// <head> ::= <head_open> <head_resource> <head_close>
//
bool HTMLParser::head() {
  bool result = false;

  if (head_open()) {
    head_resource();

    if (head_close()) {
      result = true;
    } else {
      raise_parse_error("head_close");
    }
  } else {
    raise_parse_error("head_open");
  }

  return result;
}

// <head_open> ::= "<head>"
//
bool HTMLParser::head_open() {
  advance_token();
  return check_lexeme("<head>");
}

// <head_resource> ::= ? | <style>
//
bool HTMLParser::head_resource() {
  advance_token();
  // check for optional resource
  return true;
}

// <head_close ::= "</head>"
//
bool HTMLParser::head_close() {
  advance_token();
  return check_lexeme("</head>");
}

// <body> ::= <body_open> <body_elements> <body_close>
//
bool HTMLParser::body() {
  bool result = false;

  if (body_open()) {
    while (body_elements())
      ;

    // if we get here, parsing the body elements has completed
    result = true;

    if (body_close()) {
      result = true;
    } else {
      raise_parse_error("body_close");
    }
  } else {
    raise_parse_error("body_open");
  }

  return result;
}

// <body_open> ::= "<body>"
//
bool HTMLParser::body_open() {
  advance_token();
  return check_lexeme("<body>");
}

// <body_elements> ::= (<div>)* | (<img>)* | (<text>)*
//
bool body_elements() {
  advance_token();
  return (div() || img() || text());
}

// <div> ::= <div_open> <div_close> | <div_open> <img> <div_close> | <div_open> (<text>)* <div_close>
//
bool HTMLParser::div() {
  bool result = false;

  if (div_open()) {
    if (img()) {
      if (div_close()) {
        result = true;
      } else {
        raise_parse_error("div_close");
      }
    } else if (text()) {
      if (div_close()) {
        result = true;
      } else {
        raise_parse_error("div_close");
      }
    } else if (div_close()) {
      result = true;
    } else {
      raise_parse_error("div_close");
    }
  } else {
    raise_parse_error("div_close");
  }

  return result;
}

// <div_open> ::= "<div>" | "<div " (<attr_list>)* ">"
//
bool HTMLParser::div_open() {
  bool result = false;
  advance_token();

  if (check_lexeme("<div>")) {
    result = true;
  } else if (check_lexeme("<div ")) {
    while (attr_list())
      advance_token();
    if (check_lexeme(">")) {
      result = true;
    }
  }

  return result;
}

// <div_close> ::= "</div>"
//
bool HTMLParser::div_close() {
  advance_token();
  return check_lexeme("</div>");
}

// <img> ::= "<img" (<attr_list>)* "/>"
//
bool HTMLParser::img() {
  bool result = false;
  advance_token();

  if (check_lexeme("<img ")) {
    while (attr_list())
      advance_token();
    if (check_lexeme(">")) {
      result = true;
    }
  }

  return result;
}

// <text> ::= "[a-zA-Z0-9]" | ASCII special characters
//
bool HTMLParser::text() {
  advance_token();
  return true;
}

// <attr_list> ::= <width> | <height> | <style>
//
bool HTMLParser::attr_list() {
  advance_token();
  return true;
}

// <width> ::= "width=\"<integer>\""
//
bool HTMLParser::check_width() {
  return true;
}

// <height> ::= "height=\"<integer>\""
//
bool HTMLParser::check_height() {
  return true;
}

// <body_close> ::= "</body>"
//
bool HTMLParser::body_close() {
  return true;
}

// <html_close> ::= "</html>"
//
bool HTMLParser::html_close() {
  return true;
}

}; // namespace base
