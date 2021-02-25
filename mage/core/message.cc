#include "mage/core/message.h"

#include <vector>

#include "mage/core/endpoint.h"

namespace mage {

SendInvitationMessage::SendInvitationMessage(Endpoint* endpoint_to_send) : Message(MessageType::SEND_INVITATION), endpoint_to_send_(endpoint_to_send) {}

std::vector<char> SendInvitationMessage::Serialize() {
  int payload_length = sizeof(MessageType) + (sizeof(char) * endpoint_to_send_->name.size());
  std::vector<char> buffer(payload_length);
  char* internal_buffer = buffer.data();

  // Serialize |type_|.
  type_ = MessageType::ACCEPT_INVITATION;
  memcpy(internal_buffer, (char*)&type_, sizeof(MessageType));

  // Serialize |endpoint_to_send_|'s name.
  memcpy(&internal_buffer[4], endpoint_to_send_->name.data(), endpoint_to_send_->name.size());

  buffer.push_back('\0');
  return buffer;
}

}; // namspace mage
