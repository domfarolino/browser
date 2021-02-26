#include "mage/core/util.h"

#include <string>

namespace mage {
namespace util {

// TODO(domfarolino): You can do way better than this.
std::string RandomString() {
  std::string return_string;
  for (int i = 0; i < 15; ++i) {
    return_string += alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  return return_string;
}

}; // namespace util
}; // namespace mage
