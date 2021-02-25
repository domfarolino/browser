#ifndef MAGE_CORE_MESSAGE_H_
#define MAGE_CORE_MESSAGE_H_

#include <vector>

#include "base/check.h"

namespace mage {

class Endpoint;

enum MessageType : int {
  SEND_INVITATION,
  ACCEPT_INVITATION,
};

// TODO(domfarolino): We should support variable-sized messages.
static const int kMessageHeaderSize = sizeof(int);

class Message {
 public:
  Message(MessageType type) : type_(type) {}
  virtual ~Message() = default;

  virtual std::vector<char> Serialize() = 0;

 protected:
  MessageType type_;
};

class SendInvitationMessage : public Message {
 public:
  SendInvitationMessage(Endpoint* endpoint_to_send);

  std::vector<char> Serialize() override;

 private:
  Endpoint* endpoint_to_send_;
};

}; // namspace mage

#endif // MAGE_CORE_MESSAGE_H_
