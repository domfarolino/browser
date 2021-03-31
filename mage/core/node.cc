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

MageHandle Node::SendInvitationAndGetMessagePipe(int fd) {
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

  Core::Get()->RegisterLocalHandle(local_endpoint_handle, local_endpoint);
  return local_endpoint_handle;
}

void Node::AcceptInvitation(int fd) {
  CHECK(!has_accepted_invitation_);

  std::unique_ptr<Channel> channel(new Channel(fd, this));
  channel->Start();
  node_channel_map_.insert({kInitialChannelName, std::move(channel)});

  has_accepted_invitation_ = true;
}

void Node::SendMessage(std::shared_ptr<Endpoint> local_endpoint, Message message) {
  // If we're sending a message, one of the following, but not both, must be true:
  //   1.) The peer endpoint is already in the remote node, in which case we can
  //       access the remote endpoint via the Channel in |node_channel_map_|.
  //   2.) The peer endpoint is local, held in |pending_invitations_| because we
  //       have not yet received the accept invitation message yet.
  std::string peer_node_name = local_endpoint->peer_address.node_name;
  std::string peer_endpoint_name = local_endpoint->peer_address.endpoint_name;
  auto endpoint_it = local_endpoints_.find(peer_endpoint_name);
  auto channel_it = node_channel_map_.find(peer_node_name);

  // The peer endpoint can either be local or remote, but it can't be both.
  CHECK_NE((endpoint_it == local_endpoints_.end()),
           (channel_it == node_channel_map_.end()));

  bool peer_is_local = (endpoint_it != local_endpoints_.end());
  if (peer_is_local) {
    std::shared_ptr<Endpoint> local_peer_endpoint = endpoint_it->second;
    CHECK(local_peer_endpoint);
    local_peer_endpoint->AcceptMessage(std::move(message));
  } else {
    channel_it->second->SendMessage(std::move(message));
  }
}

void Node::OnReceivedMessage(Message message) {
  switch (message.Type()) {
    case MessageType::SEND_INVITATION:
      OnReceivedInvitation(std::move(message));
      return;
    case MessageType::ACCEPT_INVITATION:
      OnReceivedAcceptInvitation(std::move(message));
      return;
    case MessageType::USER_MESSAGE:
      OnReceivedUserMessage(std::move(message));
      return;
  }

  NOTREACHED();
}

void Node::OnReceivedInvitation(Message message) {
  auto params = message.Get<SendInvitationParams>(/*starting_index=*/0);

  // Deserialize
  std::string inviter_name(
    params->inviter_name.Get()->array_storage(),
    params->inviter_name.Get()->array_storage() +
        params->inviter_name.Get()->num_elements);
  std::string temporary_remote_node_name(
    params->temporary_remote_node_name.Get()->array_storage(),
    params->temporary_remote_node_name.Get()->array_storage() +
        params->temporary_remote_node_name.Get()->num_elements);
  std::string intended_endpoint_name(
    params->intended_endpoint_name.Get()->array_storage(),
    params->intended_endpoint_name.Get()->array_storage() +
        params->intended_endpoint_name.Get()->num_elements);
  std::string intended_endpoint_peer_name(
    params->intended_endpoint_peer_name.Get()->array_storage(),
    params->intended_endpoint_peer_name.Get()->array_storage() +
        params->intended_endpoint_peer_name.Get()->num_elements);

  printf("Node::OnReceivedSendInvitation\n");
  printf("  inviter_name:                %s\n", inviter_name.c_str());
  printf("  temporary_remote_node_name:  %s\n", temporary_remote_node_name.c_str());
  printf("  intended_endpoint_name: %s\n", intended_endpoint_name.c_str());
  printf("  intended_endpoint_peer_name: %s\n", intended_endpoint_peer_name.c_str());

  // Now that we know our inviter's name, we can find our initial channel in our
  // map, and change the entry's key to the actual inviter's name.
  auto it = node_channel_map_.find(kInitialChannelName);
  CHECK_NE(it, node_channel_map_.end());
  std::unique_ptr<Channel> init_channel = std::move(it->second);
  node_channel_map_.erase(kInitialChannelName);
  node_channel_map_.insert({inviter_name, std::move(init_channel)});

  // We can also create a new local |Endpoint|, and wire it up to point to its
  // peer that we just learned about from the inviter's message.
  std::shared_ptr<Endpoint> local_endpoint(new Endpoint());
  local_endpoint->name = util::RandomString();
  local_endpoint->peer_address.node_name = inviter_name;
  local_endpoint->peer_address.endpoint_name = intended_endpoint_peer_name;
  local_endpoints_.insert({local_endpoint->name, local_endpoint});

  node_channel_map_[inviter_name]->SendAcceptInvitation(
    temporary_remote_node_name,
    name_);

  // This must come after we send the invitation acceptance above. This is
  // because the following call might immediately start sending messages to the
  // remote node, but it shouldn't receive any messages from us until it knows
  // we accepted its invitation.
  Core::Get()->OnReceivedInvitation(local_endpoint);
}

void Node::OnReceivedAcceptInvitation(Message message) {
  auto params = message.Get<SendAcceptInvitationParams>(/*starting_index=*/0);
  std::string temporary_remote_node_name(
    params->temporary_remote_node_name.Get()->array_storage(),
    params->temporary_remote_node_name.Get()->array_storage() +
        params->temporary_remote_node_name.Get()->num_elements);
  std::string actual_node_name(
    params->actual_node_name.Get()->array_storage(),
    params->actual_node_name.Get()->array_storage() +
        params->actual_node_name.Get()->num_elements);
  printf("Node::OnReceivedAcceptInvitation from: %s (actually %s)\n", temporary_remote_node_name.c_str(), actual_node_name.c_str());

  // We should only get ACCEPT_INVITATION messages from nodes that we have a
  // pending invitation for.
  auto remote_endpoint_it = pending_invitations_.find(temporary_remote_node_name);
  CHECK_NE(remote_endpoint_it, pending_invitations_.end());
  std::shared_ptr<Endpoint> remote_endpoint = remote_endpoint_it->second;

  // In order to acknowledge the invitation acceptance, we must do five things:
  //   1.) Update our local endpoint's peer address to point to the remote
  //       endpoint that we now know the full address of.
  auto local_endpoint_it = local_endpoints_.find(remote_endpoint->peer_address.endpoint_name);
  CHECK_NE(local_endpoint_it, local_endpoints_.end());
  std::shared_ptr<Endpoint> local_endpoint = local_endpoint_it->second;
  local_endpoint->peer_address.node_name = actual_node_name;
  printf("Our local endpoint now recognizes its peer as: (%s, %s)\n", local_endpoint_it->second->peer_address.node_name.c_str(), local_endpoint_it->second->peer_address.endpoint_name.c_str());

  //   2.) Remove the pending invitation from |pending_invitations_|.
  pending_invitations_.erase(temporary_remote_node_name);

  //   3.) Update |node_channel_map_| to correctly be keyed off of
  //       |actual_node_name|.
  auto node_channel_it = node_channel_map_.find(temporary_remote_node_name);
  CHECK_NE(node_channel_it, node_channel_map_.end());
  std::unique_ptr<Channel> channel = std::move(node_channel_it->second);
  node_channel_map_.erase(temporary_remote_node_name);
  node_channel_map_.insert({actual_node_name, std::move(channel)});

  //   4.) Forward any messages that were queued in |remote_endpoint| so that
  //       the remote node's endpoint gets them.
  std::queue<Message> messages_to_forward = remote_endpoint->TakeQueuedMessages();
  while (!messages_to_forward.empty()) {
    printf("    Forwarding a message\n");
    node_channel_map_[actual_node_name]->SendMessage(messages_to_forward.front());
    messages_to_forward.pop();
  }

  //   5.) Erase |remote_endpoint|. This is important, because this Endpoint
  // should no longer be used now that it is done proxying. If we leave this Endpoint in |local_endpoints_|, the next time we go to send a message to the remote endpoint, |SendMessage()| will get confused because the remote endpoint will be both local and remote at the same time, which is obviously wrong.
  CHECK_NE(local_endpoints_.find(remote_endpoint->name), local_endpoints_.end());
  local_endpoints_.erase(remote_endpoint->name);

  Core::Get()->OnReceivedAcceptInvitation();
}

void Node::OnReceivedUserMessage(Message message) {
  printf("Node::OnReceivedUserMessage\n");
  // 1. Extract the endpoint that the message is bound for.
  // TODO(domfarolino): Do this by extracting the intended endpoint name from
  // the message. The hacky way of doing it below only works because we only
  // ever have one endpoint.
  CHECK_EQ(local_endpoints_.size(), 1);
  std::shared_ptr<Endpoint> endpoint = local_endpoints_.begin()->second;
  CHECK(endpoint);

  // 2. Tell the endpoint to handle the message.
  endpoint->AcceptMessage(std::move(message));
}

}; // namespace mage
