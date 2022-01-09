#include <string>

#include "mage/core/message.h"
#include "mage/core/util.h"

namespace mage {
namespace util {

// TODO(domfarolino): You can do way better than this.
std::string RandomIdentifier() {
  std::string return_id;
  for (int i = 0; i < kIdentifierSize; ++i) {
    return_id += alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  return return_id;
}

}; // namespace util
}; // namespace mage
