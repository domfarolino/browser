#include "base/parsing/document.h"

namespace base {

Document::Document(std::string text) {
  text_ = text;
}

Document::~Document() {
  delete text_;
}

void Document::setText(std::string text) {
  text_ = text;
}

std::string Document::getText() {
  return text_;
}

}; // namespace base
