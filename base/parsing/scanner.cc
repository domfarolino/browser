#include "base/parsing/scanner.h"

Scanner::Scanner(Document *document) {
  // This could be a bad idea, but try to vectorize the whole document for now, hold my drink
  document_ = document;
  std::string text = document_.getText();
  std::stringstream sstream(text);

  while (sstream.good()) {
    std::string line;
    Token *token = new Token();

    getline(sstream, line, '\n');

    // split line, check if empty

    token->lexeme = line;
    
    token_vector_.push_back(token);
  }
};


Token* Scanner::next_token() {
  return token_vector_.pop();
}
