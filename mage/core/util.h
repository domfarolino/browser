#ifndef MAGE_CORE_UTIL_H_
#define MAGE_CORE_UTIL_H_

#include <string>

namespace mage {
namespace util {

static const char alphanum[] =
  "0123456789"
  "!@#$%^&*"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz";

std::string RandomString();

}; // namespace mage
}; // namespace util

#endif // MAGE_CORE_UTIL_H_
