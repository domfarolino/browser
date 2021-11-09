#include "base/parsing/html_parser.h"

namespace base {
HTMLParser::HTMLParser(Document document) : document_(document) {

}

HTMLParser::~HTMLParser() {

}

// Update the current token class member
void HTMLParser::advance_token() {
  current_token_ = next_token();
}

// Get the next Token from the scanner
Token HTMLParser::next_token() {
  return new Token();
}

// Check that the lexeme parsed matches
//  This function should be change to check the 
//  token class, checked against an enum
bool HTMLParser::check_lexeme(string lexeme) {
  return (strcmp(current_token_.lexeme, lexeme) == 0);
}

void HTMLParser::raise_parse_error(string token_name) {
  std::cout << "Parse Error:  Expected token " << token_name;
}

// run_parse - starts the process of parsing the document
// 
bool HTMLParser::run_parse() {
  return (document());
}


// Parser helper functions

// <document> ::= <html_open><head><body><html_close>
//
bool document() {
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
  } else {
    // need a debug log level here
    raise_parse_error("html_open");
  }

  return result;
}

// <html_open> ::= "<html>"
//
bool html_open() {
  advance_token();
  return check_lexeme("<html>");
}

// <head> ::= <head_open> <head_resource> <head_close>
//
bool head() {
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
bool head_open() {
  advance_token();
  return check_lexeme("<head>");
}

// <head_resource> ::= ? | <style>
//
bool head_resource() {
  advance_token();
  // check for optional resource
  return true;
}

// <head_close ::= "</head>"
//
bool head_close() {
  advance_token();
  return check_lexeme("</head>");
}

// <body> ::= <body_open> <body_elements> <body_close>
//
bool body() {
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
bool body_open() {
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
bool div() {
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
  } else {
    raise_parse_error("div_close");
  }

  return result;
}

// <div_open> ::= "<div>" | "<div " (<attr_list>)* ">"
//
bool div_open() {
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
bool div_close() {
  advance_token();
  return check_lexeme("</div>");
}

// <img> ::= "<img" (<attr_list>)* "/>"
//
bool img() {
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
bool text() {
  advance_token();
  return true;
}

// <attr_list> ::= <width> | <height> | <style>
//
bool attr_list() {
  advance_token();
  return true;
}

// <width> ::= "width=\"<integer>\""
//
bool check_width() {

}

// <height> ::= "height=\"<integer>\""
//
bool check_height() {

}

// <body_close> ::= "</body>"
//
bool body_close() {

}

// <html_close> ::= "</html>"
//
bool html_close() {

}

}; // namespace base
