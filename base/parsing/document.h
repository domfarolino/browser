#ifndef BASE_PARSING_DOCUMENT_H_
#define BASE_PARSING_DOCUMENT_H_

#include <iostream>
#include <string>

namespace base {
// |Document| is a class that represents the document retrieved from an 
// HTTP Response from the network, and represents a document before it 
// has been parsed.
class Document {
  public:
    Document(std::string text = "") : text_(text);
    ~Document();

    void setText(std::string text);
    std::string getText();

  private:
    std::string text_;
};
}; // namespace base

#endif // BASE_PARSING_DOCUMENT_H_
