#ifndef MAGE_CORE_MESSAGE_H_
#define MAGE_CORE_MESSAGE_H_

#include <string>
#include <vector>

#include "base/check.h"

namespace mage {

enum MessageType : int {
  // This sends the maiden message to a new peer node along with a bootstrap
  // endpoint identifier that the peer node will add. The peer node is then
  // expected to send the next message below. The inviter's boostrap endpoint is
  // local and ephemeral, used to queue messages that will eventually
  // make it to the fresh peer node. The state of things in the inviter node
  // after sending this looks like something like:
  // +------------------------------+
  // |           Node 1             |
  // | +--------+       +---------+ |
  // | |Local   |       |Ephemeral| |
  // | |Endpoint|       |Endpoint | |
  // | |  peer-+|------>|         | |
  // | |        |<------|-+peer   | |
  // | +--------+       +---------+ |
  // +-------------------------------+
  SEND_INVITATION,
  // A fresh peer node will send this message to its inviter node after
  // receiving the above message and recovering the ephemeral endpoint
  // identifier from it. Once the fresh peer recovers the endpoint identifier,
  // it adds it as a local endpoint, whose peer is "Local Endpoint" above
  // When the inviter receives this message, it knows to do two things:
  //   1.) Set "Local Endpoint"'s  peer to the remote endpoint in the new peer
  //       node
  //   2.) Set "Ephemeral Endpoint" in a proxying state, flushing all messages
  //       it may have queued to the remote endpoint in the peer node
  // At this point, chain of endpoints looks like so:
  //     +-------------------------------+
  //     |                               |
  //     v                   Proxying    +
  //   Local      Ephemeral+---------> Remote
  //  Endpoint     Endpoint           Endpoint
  //     +                               ^
  //     |                               |
  //     +-------------------------------+
  //     
  //     ^                 ^             ^
  //     |                 |             |
  //     +------Node 1-----+-----Node2---+
  ACCEPT_INVITATION,
  // TODO(domfarolino): Figure out if we need any more messages, like one
  // indicating that all proxies have been closed and RemoteEndpoint will no
  // longer be getting messages from endpoints other than its peer. This could
  // be useful, but before implementing it, it's not clear if it is necessary.
  USER_MESSAGE,
};

class Message {
 public:
  Message(MessageType type) : type_(type) {}
  virtual ~Message() = default;

  virtual std::vector<char> Serialize() = 0;
  static std::unique_ptr<Message> Deserialize(int fd) {
    NOTREACHED();
  }

 protected:
  MessageType type_;
};

class SendInvitationMessage : public Message {
 public:
  SendInvitationMessage() : Message(MessageType::SEND_INVITATION) {}

  std::vector<char> Serialize() override;
  static std::unique_ptr<Message> Deserialize(int fd);

  // TODO(domfarolino): We should either make this a struct and have these be public, or make these private and give these setters.
  std::string inviter_name_;
  std::string temporary_remote_node_name_;
  std::string intended_peer_endpoint_name_;
};

}; // namspace mage

#endif // MAGE_CORE_MESSAGE_H_
