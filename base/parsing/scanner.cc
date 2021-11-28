#include "base/parsing/scanner.h"

namespace base {

Scanner::Scanner(Document *document) {
  token_vector_ = std::vector<std::unique_ptr<Token>>();

  // This could be a bad idea, but try to vectorize the whole document for now, hold my drink
  document_ = document;
  std::string text = document_->getText();
  std::stringstream sstream(text);

  while (sstream.good()) {
    std::string line;
    auto token = new Token("", "");

    getline(sstream, line, '\n');

    // split line, check if empty

    token->lexeme = line;
    
    // token_vector_.push_back(token);
    token_vector_.emplace_back(token);
  }
};


std::unique_ptr<Token> Scanner::next_token() {
  auto token = std::move(token_vector_.back());
  token_vector_.pop_back();
  return token;
}

}; // namespace base
