#include "base/parsing/scanner.h"

namespace base {

Scanner::Scanner(Document *document) {
  token_vector_ = std::vector<std::shared_ptr<Token>>();

  // This could be a bad idea, but try to vectorize the whole document for now, hold my drink
  document_ = document;
  std::string text = document_->getText();
  std::stringstream sstream(text);

  while (sstream.good()) {
    std::string line;

    // split each line
    std::getline(sstream, line, '\n');

    // parse line
    while (line.length() > 0) {
      int token_length = 0;

      // create a substring of the token length
      while ((line[token_length] != NULL) && (line[token_length] != '>')) token_length++;

      if (token_length > 0) {
        auto token = std::make_shared<Token>(line.substr(0, token_length + 1), "");
        token_vector_.emplace_back(token);
      }

      line = line.substr(token_length + 1, line.length());
    }
  }
};


std::shared_ptr<Token> Scanner::next_token() {
  auto token = token_vector_.front();
  // change this to queue/dequeue
  token_vector_.erase(token_vector_.begin());
  return token;
}

}; // namespace base
