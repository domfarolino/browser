#ifndef MAGE_CORE_ENDPOINT_H_
#define MAGE_CORE_ENDPOINT_H_

#include <string>
#include <queue>

#include "mage/core/message.h"

namespace mage {

struct Address {
  std::string node_name;
  std::string endpoint_name;
};

class Endpoint {
 public:
  enum class State {
    kUnbound, // Queueing
    kBound,
  };

  Endpoint() : state(State::kUnbound) {}
  Endpoint(const Endpoint&) = delete;
  Endpoint& operator=(const Endpoint&) = delete;

  std::string name;
  Address peer_address;

  State state;

  // This is used when the port isn't bound to a local interface. Once bound,
  // these messages will be forwarded, in order, to the delegate (likely some
  // receiver that just got bound).
  std::queue<std::unique_ptr<Message>> incoming_message_queue;
};

}; // namspace mage

#endif // MAGE_CORE_ENDPOINT_H_
