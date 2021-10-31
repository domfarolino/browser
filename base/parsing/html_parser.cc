#include "base/parsing/html_parser.h"

namespace base {
HTMLParser::HTMLParser(Document document) : document_(document) {

}

HTMLParser::~HTMLParser() {

}

// Check the token class
// 
Token HTMLParser::next_token() {

}

// Simplified Grammar for HTMLParser
//
// <document> ::= <html_open><head><body><html_close>
//
// <html_open> ::= "<html>"
//
// <head> ::= <head_open> <head_resource> <head_close>
//
// <head_open> ::= "<head>"
//
// <head_resource> ::= ? | <style>
//
// <head_close ::= "</head>"
//
// <body> ::= <body_open> <body_elements> <body_close>
//
// <body_open> ::= "<body>"
//
// <body_elements> ::= (<div>)* | (<img>)* | (<text>)*
//
// <div> ::= <div_open> <div_close> | div_open> <img> <div_close> | <div_open> (<text>)* <div_close>
//
// <div_open> ::= "<div>" | "<div " (<attr_list>)* ">"
//
// <div_close> ::= "</div>"
//
// <img> ::= "<img" (<attr_list>)* "/>"
//
// <text> ::= "[a-zA-Z0-9]" | ASCII special characters
//
// <attr_list> ::= <width> | <height> | <style>
//
// <width> ::= "width=\"<integer>\""
//
// <height> ::= "height=\"<integer>\""
//
// <integer> ::= ([0-9])*
//
// <style> ::= <css_style_document>
//
// <body_close> ::= "</body>"
//
// <html_close> ::= "</html>"
bool HTMLParser::run_parse() {
  return (document());
}


// Parser helper functions

// <document> ::= <html_open><head><body><html_close>
//
bool document() {
  bool result = false;

  if (html_open()) {
    next_token();

    // Should make these optional
    if (head()) {
      next_token();

      if (body()) {
        next_token();

        if (html_close()) {
          result = true;
        }
      }
    }
  }

  return result;
}

// <html_open> ::= "<html>"
//
bool html_open() {
  bool result = false;
  Token token = next_token();

  return (strcmp(token.lexeme, "<html>") == 0);
}

// <head> ::= <head_open> <head_resource> <head_close>
//
bool head() {

}

// <head_open> ::= "<head>"
//
bool head_open() {

}

// <head_resource> ::= ? | <style>
//
bool head_resource() {

}

// <head_close ::= "</head>"
//
bool head_close() {

}

// <body> ::= <body_open> <body_elements> <body_close>
//
bool body() {

}

// <body_open> ::= "<body>"
//
bool body_open() {

}

// <body_elements> ::= (<div>)* | (<img>)* | (<text>)*
//
bool body_elements() {

}

// <div> ::= <div_open> <div_close> | div_open> <img> <div_close> | <div_open> (<text>)* <div_close>
//
bool div() {

}

// <div_open> ::= "<div>" | "<div " (<attr_list>)* ">"
//
bool div_open() {

}

// <div_close> ::= "</div>"
//
bool div_close() {

}

// <img> ::= "<img" (<attr_list>)* "/>"
//
bool img() {

}

// <text> ::= "[a-zA-Z0-9]" | ASCII special characters
//
bool text() {

}

// <attr_list> ::= <width> | <height> | <style>
//
bool attr_list() {

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
