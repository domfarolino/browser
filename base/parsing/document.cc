#include "base/parsing/document.h"

namespace base {

void Document::setText(std::string text) {
  text_ = text;
}

std::string Document::getText() {
  return text_;
}

}; // namespace base
