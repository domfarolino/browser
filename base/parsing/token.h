#ifndef BASE_PARSING_TOKEN_H_
#define BASE_PARSING_TOKEN_H_

#include <string>

namespace base {

// enum class TokenClass {

// };

// |Token| represents a lexeme after it has been scanned by 
// the Scanner to determine it's token class
class Token {
  public:
    Token(std::string lexeme, std::string token_class) : lexeme(lexeme), token_class(token_class) {};
    ~Token(){}

    std::string lexeme;
    std::string token_class;

  private:
};
}; // namespace base

#endif // BASE_PARSING_TOKEN_H_
