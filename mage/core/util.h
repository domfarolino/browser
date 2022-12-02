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

std::string RandomIdentifier();

#define VA_ARGS(...) , ##__VA_ARGS__
// Log a message on a single line with no newline appended.
#define LOG_SL(str, ...)   printf(str VA_ARGS(__VA_ARGS__))
// Log a message with a new line appended to the end.
#define LOG(str, ...)      printf(str VA_ARGS(__VA_ARGS__));printf("\n")

}; // namespace mage
}; // namespace util

#endif // MAGE_CORE_UTIL_H_
