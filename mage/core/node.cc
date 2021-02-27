#include "mage/core/node.h"

#include <memory>

#include "mage/core/core.h"
#include "mage/core/endpoint.h"
#include "mage/core/message.h"

namespace mage {

void Node::InitializeAndEntangleEndpoints(std::shared_ptr<Endpoint> ep1, std::shared_ptr<Endpoint> ep2) {
  // Initialize the endpoints.
  ep1->name = util::RandomString();
  ep2->name = util::RandomString();

  // Both endpoints are local initially.
  ep1->peer_address.node_name = name_;
  ep2->peer_address.node_name = name_;

  // Entangle endpoints.
  ep1->peer_address.endpoint_name = ep2->name;
  ep2->peer_address.endpoint_name = ep1->name;

  local_endpoints_.insert({ep1->name, ep1});
  local_endpoints_.insert({ep2->name, ep2});

  printf("Initialized and entangled the following endpoints:\n"
         "  endpoint1: (%s, %s)\n"
         "  endpoint2: (%s, %s)\n", name_.c_str(), ep1->name.c_str(),
         name_.c_str(), ep2->name.c_str());
}

MageHandle Node::SendInvitationToTargetNodeAndGetMessagePipe(int fd) {
  // This node wishes to invite another fresh peer node to the network of
  // processes. The sequence of events here looks like so:
  //   Endpoints:
  //     1.) Create two new entangled endpoints.
  //     2.) Insert both endpoints in the |local_endpoints_| map since both
  //         endpoints are technically local to |this| at this point. Eventually
  //         only one endpoint, |local_endpoint| below will be local to |this|
  //         while |remote_endpoint| will eventually be replaced with an
  //         endpoint with the same name as |remote_endpoint| on the target node.
  //     3.) Insert |remote_endpoint| into |reserved_endpoints| which maps
  //         temporary node names to endpoints that will eventually be replaced
  //         with a truly remote endpoint.
  //   Nodes:
  //     1.) Create a new |Channel| for the node whose name is
  //         |temporary_remote_node_name| (its name will change later when it
  //         tells us its real name).
  //     2.) Start the channel (listen for any messages from the remote node,
  //         though we don't expect any just yet).
  //     3.) Send the |MessageType::SEND_INVITATION| message to the remote node.
  //     4.) Store the channel in |node_channel_map_| so that we can reference
  //         it whenever we send a message from an endpoint that is bound for
  //         the remote node.
  //     5.) Insert the temporary remote node name in the |pending_invitations_|
  //         set. When we receive invitation acceptance later on, we want to
  //         make sure we know which node we're receiving the acceptance from.
  //         We use this set to keep track of our pending invitations that we
  //         expect acceptances for. Later we'll update all instances of
  //         |temporary_remote_node_name| to the actual remote node name that it
  //         makes us aware of as a part of invitation acceptance.
  //   MageHandles:
  //     1.) Return the |MageHandle| assocaited with |local_endpoint| so that
  //         this process can start immediately queueing messages on
  //         |local_endpoint| that will eventually be delivered to the remote
  //         process.

  std::shared_ptr<Endpoint> local_endpoint(new Endpoint()), remote_endpoint(new Endpoint());
  InitializeAndEntangleEndpoints(local_endpoint, remote_endpoint);

  MageHandle local_endpoint_handle = Core::Get()->GetNextMageHandle();

  NodeName temporary_remote_node_name = util::RandomString();

  std::unique_ptr<Channel> channel(new Channel(fd, /*delegate=*/this));
  channel->Start();
  channel->SetRemoteNodeName(temporary_remote_node_name);
  channel->SendInvitation(name_, remote_endpoint->name, remote_endpoint->peer_address.endpoint_name);

  node_channel_map_.insert({temporary_remote_node_name, std::move(channel)});
  pending_invitations_.insert({temporary_remote_node_name, remote_endpoint});
  return local_endpoint_handle;
}

void Node::OnReceivedMessage(std::unique_ptr<Message> message) {
  printf("Node::OnReceivedMessage!\n");
  switch (message->type) {
    case MessageType::SEND_INVITATION:
      OnReceivedInvitation(std::move(message));
      return;
    case MessageType::ACCEPT_INVITATION:
      NOTREACHED();
      OnReceivedAcceptInvitation(std::move(message));
      return;
    case MessageType::USER_MESSAGE:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

void Node::OnReceivedInvitation(std::unique_ptr<Message> message) {
  SendInvitationMessage* inner_invitation = static_cast<SendInvitationMessage*>(message.release());
  std::unique_ptr<SendInvitationMessage> invitation(inner_invitation);

  printf("Node::OnReceivedSendInvitation\n");
  printf("  inviter_name:                %s\n", invitation->inviter_name.c_str());
  printf("  temporary_remote_node_name:  %s\n", invitation->temporary_remote_node_name.c_str());
  printf("  intended_endpoint_name: %s\n", invitation->intended_endpoint_name.c_str());
  printf("  intended_endpoint_peer_name: %s\n", invitation->intended_endpoint_peer_name.c_str());

  // Now that we know our inviter's name, we can find our initial channel in our
  // map, and change the entry's key to the actual inviter's name.
  auto it = node_channel_map_.find(kInitialChannelName);
  CHECK_NE(it, node_channel_map_.end());
  std::unique_ptr<Channel> init_channel = std::move(it->second);
  node_channel_map_.erase(kInitialChannelName);
  node_channel_map_.insert({invitation->inviter_name, std::move(init_channel)});

  // We can also create a new local |Endpoint|, and wire it up to point to its
  // peer that we just learned about from the inviter's message.
  std::shared_ptr<Endpoint> local_endpoint(new Endpoint());
  local_endpoint->name = util::RandomString();
  local_endpoint->peer_address.node_name = invitation->inviter_name;
  local_endpoint->peer_address.endpoint_name = invitation->intended_endpoint_peer_name;
  local_endpoints_.insert({local_endpoint->name, local_endpoint});

  printf("Calling into core now\n");
  Core::Get()->OnReceivedInvitation(local_endpoint);
}

void Node::OnReceivedAcceptInvitation(std::unique_ptr<Message> message) {}


void Node::AcceptInvitation(int fd) {
  CHECK_EQ(has_accepted_invitation_, false);

  std::unique_ptr<Channel> channel(new Channel(fd, this));
  channel->Start();
  node_channel_map_.insert({kInitialChannelName, std::move(channel)});

  has_accepted_invitation_ = true;
}

}; // namespace mage
