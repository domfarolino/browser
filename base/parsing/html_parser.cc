#include "base/parsing/html_parser.h"

namespace base {
// <document> ::= <html_open><head><body><html_close>
//
// <html_open> ::= "<html>"
//
// <head> ::= <head_open> <head_resource> <head_close>
//
// <head_open> ::= "<head>"
//
// <head_resource> ::= ?
//
// <head_close ::= "</head>"
//
// <body> ::= <body_open> <body_elements> <body_close>
//
// <body_open> ::= "<body>"
//
// <body_elements> ::= (<div>)* | (<img>)* | (<text>)*
//
// <div> ::= <div_open> <div_close>
//
// <div_open> ::= "<div>"
//
// <div_close> ::= "</div>"
//
// <img> ::= "<img" (attr_list)* "/>"
//
// <text> ::= "[a-zA-Z0-9]" | ASCII special characters
//
// <body_close> ::= "</body>"
//
// <html_close> ::= "</html>"
}; // namespace base
