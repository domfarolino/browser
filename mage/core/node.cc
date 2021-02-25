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
  // Attach the partial message pipe to the invitation, and prepare its handle
  // for return. Once returned, it is immediately usable by this process to
  // send messages that will eventually arrive on the remote process.
  std::shared_ptr<Endpoint> local_endpoint(new Endpoint()), remote_endpoint(new Endpoint());
  InitializeAndEntangleEndpoints(local_endpoint, remote_endpoint);

  MageHandle local_endpoint_handle = Core::Get()->GetNextMageHandle();

  NodeName temporary_remote_node_name = util::RandomString();
  reserved_endpoints_.insert({temporary_remote_node_name, remote_endpoint});

  std::unique_ptr<Channel> channel(new Channel(fd));
  channel->Start();
  channel->SetRemoteNodeName(temporary_remote_node_name);
  channel->SendInvitation(remote_endpoint.get());

  node_channel_map_.insert({temporary_remote_node_name, std::move(channel)});

  // This is the part where we send an invitation to the remote process.
  // Node:
  //   X.) Create a new channel for the remote node
  //   X.) Add to reate a new channel and put it in pending_invitations or pending_peers or something.
  //   3.) Create a new Channel given fd
  //   X.) Invoke channel->SetRemoteNodeName()
  //   4.) Invoke channel->SendInvitation(...)
  // NOTREACHED();
  return local_endpoint_handle;
}

void Node::AcceptInvitation(int fd) {
  std::unique_ptr<Channel> channel(new Channel(fd));
  channel->Start();
  std::string temporary_remote_node_name = "test"; // fix this!! We should just make this the inviter channel or something.
  node_channel_map_.insert({temporary_remote_node_name, std::move(channel)});
}

void Node::WriteMessage(MessageType message_type) {
  switch (message_type) {
    case MessageType::SEND_INVITATION:
      printf("Will be writing an SEND_INVITATION message\n");
    case MessageType::ACCEPT_INVITATION:
      printf("Will be writing an ACCEPT_INVITATION message\n");
  }
}

}; // namespace mage
