#ifndef MAGE_CORE_NODE_H_
#define MAGE_CORE_NODE_H_

#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <queue>

#include "base/check.h"
#include "mage/core/channel.h"
#include "mage/core/handles.h"
#include "mage/core/message.h"
#include "mage/core/util.h"

namespace mage {

class Endpoint;

static const std::string kInitialChannelName = "INIT";

class Node : public Channel::Delegate {
 public:
  Node() : name_(util::RandomString()) {
    printf("Node name_: %s\n", name_.c_str());
  }

  void InitializeAndEntangleEndpoints(std::shared_ptr<Endpoint> ep1, std::shared_ptr<Endpoint> ep2);
  MageHandle SendInvitationToTargetNodeAndGetMessagePipe(int fd);
  void AcceptInvitation(int fd);

  // Channel::Delegate implementation:
  void OnReceivedMessage(std::unique_ptr<Message> message) override;

  void OnReceivedSendInvitation(std::unique_ptr<Message>);
  void OnReceivedAcceptInvitation(std::unique_ptr<Message>);

 private:
  std::string name_;

  // True once |this| accepts an invitation from an inviter node.
  bool has_accepted_invitation_ = false;

  // To more easily reason about the below data structures.
  using NodeName = std::string;
  using EndpointName = std::string;

  // TODO(domfarolino): Once we support passing endpoints over existing
  // endpoints, it will be possible for a node to be given an endpoint to a node
  // that it doesn't know of yet. We'll need a map of queues like the following
  // to hold messages destined to peers that belong to nodes we don't know yet.
  // We may also be able to hold these queues on the local |Endpoint|s
  // themselves.
  // std::map<NodeName, std::map<EndpointName, std::queue<std::unique_ptr<Message>>>> pending_outgoing_endpoint_messages_;

  // All endpoints whose address's "node name" is our |name_|
  std::map<EndpointName, std::shared_ptr<Endpoint>> local_endpoints_;

  std::map<NodeName, std::unique_ptr<Channel>> node_channel_map_;

  // TODO(domfarolino): Can we merge the following two?
  // A set of |NodeNames| that we've sent invitations to, and are awaiting acceptances from. We need this to keep track of which node accepts which invitation.
  std::set<NodeName> pending_invitations_;
  // Holds the remote endpoint associated with a given (temporary) node name
  // that we're sending an invite to. Once an invitation acceptance comes back
  // from the node, we update instances of its temporary name with its real one
  // that it tells us, and then [...]
  std::map<NodeName, std::shared_ptr<Endpoint>> reserved_endpoints_;
};

}; // namespace mage

#endif // MAGE_CORE_NODE_H_