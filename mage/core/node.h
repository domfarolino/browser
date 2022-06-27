#ifndef MAGE_CORE_NODE_H_
#define MAGE_CORE_NODE_H_

#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <queue>

#include "gtest/gtest_prod.h"
#include "mage/core/channel.h"
#include "mage/core/handles.h"
#include "mage/core/message.h"
#include "mage/core/util.h"

namespace mage {

class Endpoint;

static const std::string kInitialChannelName = "INIT";

class Node : public Channel::Delegate {
 public:
  Node() : name_(util::RandomIdentifier()) {
    printf("\n\nNode name_: %s\n", name_.c_str());
  }
  ~Node() = default;

  std::vector<MageHandle> CreateMessagePipes();
  MageHandle SendInvitationAndGetMessagePipe(int fd);
  void AcceptInvitation(int fd);
  void SendMessage(std::shared_ptr<Endpoint> local_endpoint, Message message);

  // Channel::Delegate implementation:
  void OnReceivedMessage(Message message) override;

  void OnReceivedInvitation(Message message);
  void OnReceivedAcceptInvitation(Message message);
  void OnReceivedUserMessage(Message message);

 private:
  // TODO(domfarolino): This is a bit nasty. Can we remove this.
  friend class Core;
  friend class MageTest;
  FRIEND_TEST(MageTest, InitializeAndEntangleEndpointsUnitTest);

  std::vector<std::pair<MageHandle, std::shared_ptr<Endpoint>>>
      CreateMessagePipesAndGetEndpoints();
  void InitializeAndEntangleEndpoints(std::shared_ptr<Endpoint> ep1,
                                      std::shared_ptr<Endpoint> ep2) const;

  std::string name_;

  // True once |this| accepts an invitation from an inviter node.
  bool has_accepted_invitation_ = false;

  // To more easily reason about the below data structures.
  using NodeName = std::string;
  using EndpointName = std::string;

  // All endpoints that are local to this node, that is, whose address's
  // "node name" is our |name_|.
  std::map<EndpointName, std::shared_ptr<Endpoint>> local_endpoints_;

  // Used when we send a message from a (necessarily, local) endpoint in order
  // to find the channel associated with its peer endpoint. Without this, we
  // couldn't send remote messages. All node names in this map will never be our
  // own |name_| since this is only used for remote nodes. Messages to an
  // endpoint in the same node (that is, from an endpoint in Node A to its peer
  // endpoint also in Node A) go through a different path.
  std::map<NodeName, std::unique_ptr<Channel>> node_channel_map_;

  // Maps |NodeNames| that we've sent invitations to and are awaiting
  // acceptances from, to an |Endpoint| that we've reserved for the peer node.
  // The node names in this map are the temporary one we generate for a peer
  // node before it has told us its real name. Once an invitation acceptance
  // comes back from the node that identifies itself to us by the temporary name
  // we've given it, we update instances of its temporary name with its "real"
  // one that it provides in the invitation acceptance message.
  std::map<NodeName, std::shared_ptr<Endpoint>> pending_invitations_;

  // TODO(domfarolino): Once we support passing endpoints over existing
  // endpoints, it will be possible for a node to be given an endpoint to a node
  // that it doesn't know of yet. We'll need a map of queues like the following
  // to hold messages destined to peers that belong to nodes we don't know yet.
  // We may also be able to hold these queues on the local |Endpoint|s
  // themselves.
  // std::map<NodeName, std::map<EndpointName, std::queue<Message>>> pending_outgoing_endpoint_messages_;
};

}; // namespace mage

#endif // MAGE_CORE_NODE_H_
