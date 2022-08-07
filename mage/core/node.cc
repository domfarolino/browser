#include "mage/core/node.h"

#include <memory>

#include "base/check.h"
#include "base/threading/thread_checker.h" // for CHECK_ON_THREAD().
#include "mage/core/core.h"
#include "mage/core/endpoint.h"
#include "mage/core/message.h"

namespace mage {

void Node::InitializeAndEntangleEndpoints(std::shared_ptr<Endpoint> ep1,
                                          std::shared_ptr<Endpoint> ep2) const {
  CHECK_ON_THREAD(base::ThreadType::UI);
  // Initialize the endpoints.
  ep1->name = util::RandomIdentifier();
  ep2->name = util::RandomIdentifier();

  // Both endpoints are local initially.
  ep1->peer_address.node_name = name_;
  ep2->peer_address.node_name = name_;

  // Entangle endpoints.
  ep1->peer_address.endpoint_name = ep2->name;
  ep2->peer_address.endpoint_name = ep1->name;

  printf("Initialized and entangled the following endpoints:\n"
         "  endpoint1: (%s, %s)\n"
         "  endpoint2: (%s, %s)\n", name_.c_str(), ep1->name.c_str(),
         name_.c_str(), ep2->name.c_str());
}

std::vector<MageHandle> Node::CreateMessagePipes() {
  CHECK_ON_THREAD(base::ThreadType::UI);
  std::vector<std::pair<MageHandle, std::shared_ptr<Endpoint>>>
      pipes_and_endpoints = Node::CreateMessagePipesAndGetEndpoints();
  return {pipes_and_endpoints[0].first, pipes_and_endpoints[1].first};
}

std::vector<std::pair<MageHandle, std::shared_ptr<Endpoint>>>
Node::CreateMessagePipesAndGetEndpoints() {
  CHECK_ON_THREAD(base::ThreadType::UI);
  std::shared_ptr<Endpoint> endpoint_1(new Endpoint()),
                            endpoint_2(new Endpoint());
  InitializeAndEntangleEndpoints(endpoint_1, endpoint_2);
  MageHandle handle_1 = Core::Get()->GetNextMageHandle(),
             handle_2 = Core::Get()->GetNextMageHandle();
  Core::Get()->RegisterLocalHandleAndEndpoint(handle_1, endpoint_1);
  Core::Get()->RegisterLocalHandleAndEndpoint(handle_2, endpoint_2);
  CHECK_NE(local_endpoints_.find(endpoint_1->name), local_endpoints_.end());
  CHECK_NE(local_endpoints_.find(endpoint_2->name), local_endpoints_.end());
  return {std::make_pair(handle_1, endpoint_1), std::make_pair(handle_2, endpoint_2)};
}

MageHandle Node::SendInvitationAndGetMessagePipe(int fd) {
  CHECK_ON_THREAD(base::ThreadType::UI);
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

  std::vector<std::pair<mage::MageHandle, std::shared_ptr<Endpoint>>>
      pipes = CreateMessagePipesAndGetEndpoints();
  std::shared_ptr<Endpoint> local_endpoint = pipes[0].second,
                            remote_endpoint = pipes[1].second;

  NodeName temporary_remote_node_name = util::RandomIdentifier();

  std::unique_ptr<Channel> channel(new Channel(fd, /*delegate=*/this));
  channel->Start();
  channel->SetRemoteNodeName(temporary_remote_node_name);
  channel->SendInvitation(/*inviter_name=*/name_,
                          /*intended_endpoint_peer_name=*/
                            remote_endpoint->peer_address.endpoint_name);

  node_channel_map_.insert({temporary_remote_node_name, std::move(channel)});
  pending_invitations_.insert({temporary_remote_node_name, remote_endpoint});

  MageHandle local_endpoint_handle = pipes[0].first;
  return local_endpoint_handle;
}

void Node::AcceptInvitation(int fd) {
  CHECK(!has_accepted_invitation_);

  printf("Node::AcceptInvitation() getpid: %d\n", getpid());
  std::unique_ptr<Channel> channel(new Channel(fd, this));
  channel->Start();
  node_channel_map_.insert({kInitialChannelName, std::move(channel)});

  has_accepted_invitation_ = true;
}

void Node::SendMessage(std::shared_ptr<Endpoint> local_endpoint,        
                       Message message) {
  printf("Node::SendMessage() [pid=%d]\n", getpid());
  CHECK_EQ(message.GetMutableMessageHeader().type, MessageType::USER_MESSAGE);
  // If we're sending a message, one of the following, but not both, must be
  // true:
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

  // We have to write the `peer_endpoint_name` to the message, so that the
  // ultimate recipient can dispatch it correctly.
  CHECK_EQ(peer_endpoint_name.size(), kIdentifierSize);
  memcpy(message.GetMutableMessageHeader().target_endpoint, peer_endpoint_name.c_str(), kIdentifierSize);

  bool peer_is_local = (endpoint_it != local_endpoints_.end());
  printf(" peer_is_local: %d\n", getpid());
  if (peer_is_local) {
    std::shared_ptr<Endpoint> local_peer_endpoint = endpoint_it->second;
    CHECK(local_peer_endpoint);

    if (local_peer_endpoint->state == Endpoint::State::kUnboundAndProxying) {
      std::string actual_node_name = local_peer_endpoint->proxy_target.node_name;
      std::string actual_endpoint_name = local_peer_endpoint->proxy_target.endpoint_name;
      printf("  local_peer is in proxying state. forwarding message to remote endpoint (%s : %s)\n",
             actual_node_name.c_str(), actual_endpoint_name.c_str());

      // If we know that this message is supposed to be proxied to another node,
      // we have to rewrite its target endpoint to be the proxy target. We do
      // the same for dependant endpoint descriptors in
      // `SendMessagesAndRecursiveDependants()`.
      memcpy(message.GetMutableMessageHeader().target_endpoint, actual_endpoint_name.c_str(), kIdentifierSize);

      std::queue<Message> messages_to_send;
      messages_to_send.push(std::move(message));
      SendMessagesAndRecursiveDependants(std::move(messages_to_send), local_peer_endpoint);
      return;
    }

    printf("  local_peer is not proxying state, going to deliver the message right there\n");
    // If `local_peer_endpoint` is in the `kUnboundAndQueueing` or `kBound`
    // state, than we can just pass this single message to the peer without
    // recursively looking at dependant messages. That's because if we *did*
    // recursively look through all of the dependant messages and try and
    // forward them, we'd just be forwarding them to *their* local peers in the
    // same node, which is where those messages already are.
    local_peer_endpoint->AcceptMessageOnDelegateThread(std::move(message));
  } else {
    channel_it->second->SendMessage(std::move(message));
  }
}

void Node::OnReceivedMessage(Message message) {
  CHECK_ON_THREAD(base::ThreadType::IO);

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
  CHECK_ON_THREAD(base::ThreadType::IO);
  SendInvitationParams* params = message.GetView<SendInvitationParams>();

  // Deserialize
  std::string inviter_name(params->inviter_name,
                           params->inviter_name + kIdentifierSize);
  std::string temporary_remote_node_name(
      params->temporary_remote_node_name,
      params->temporary_remote_node_name + kIdentifierSize);
  std::string intended_endpoint_peer_name(
      params->intended_endpoint_peer_name,
      params->intended_endpoint_peer_name + kIdentifierSize);

  printf("Node::OnReceivedInvitation getpid(): %d\n", getpid());
  printf("  inviter_name:                %s\n", inviter_name.c_str());
  printf("  temporary_remote_node_name:  %s\n",
                temporary_remote_node_name.c_str());
  printf("  intended_endpoint_peer_name: %s\n",
                intended_endpoint_peer_name.c_str());

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
  // Choose a random name for this endpoint. We send this name back to the
  // inviter when we accept the invitation.
  local_endpoint->name = util::RandomIdentifier();
  local_endpoint->peer_address.node_name = inviter_name;
  local_endpoint->peer_address.endpoint_name = intended_endpoint_peer_name;
  local_endpoints_.insert({local_endpoint->name, local_endpoint});
  printf("  local_endpoint->name: %s\n", local_endpoint->name.c_str());
  printf("  local_endpoint->peer_address.node_name: %s\n", local_endpoint->peer_address.node_name.c_str());
  printf("  local_endpoint->peer_address.endpoint_name: %s\n", local_endpoint->peer_address.endpoint_name.c_str());

  node_channel_map_[inviter_name]->SendAcceptInvitation(
      temporary_remote_node_name, name_, local_endpoint->name);

  // This must come after we send the invitation acceptance above. This is
  // because the following call might immediately start sending messages to the
  // remote node, but it shouldn't receive any messages from us until it knows
  // we accepted its invitation.
  Core::Get()->OnReceivedInvitation(local_endpoint);
}

void Node::OnReceivedAcceptInvitation(Message message) {
  CHECK_ON_THREAD(base::ThreadType::IO);
  SendAcceptInvitationParams* params =
      message.GetView<SendAcceptInvitationParams>();
  std::string temporary_remote_node_name(
      params->temporary_remote_node_name,
      params->temporary_remote_node_name + kIdentifierSize);
  std::string actual_node_name(params->actual_node_name,
                               params->actual_node_name + kIdentifierSize);
  std::string accept_invitation_endpoint_name(
      params->accept_invitation_endpoint_name,
      params->accept_invitation_endpoint_name + kIdentifierSize);

  printf("Node::OnReceivedAcceptInvitation [getpid(): %d] from: %s (actually %s)\n",
         getpid(), temporary_remote_node_name.c_str(), actual_node_name.c_str());

  // We should only get ACCEPT_INVITATION messages from nodes that we have a
  // pending invitation for.
  auto remote_endpoint_it =
    pending_invitations_.find(temporary_remote_node_name);
  CHECK_NE(remote_endpoint_it, pending_invitations_.end());
  std::shared_ptr<Endpoint> remote_endpoint = remote_endpoint_it->second;

  // In order to acknowledge the invitation acceptance, we must do four things:
  //   1.) Put |remote_endpoint| in the `kUnboundAndProxying` state, so that
  //       when `SendMessage()` gets message bound for it, it knows to forward
  //       them to the appropriate remote node.
  CHECK_NE(local_endpoints_.find(remote_endpoint->name),
           local_endpoints_.end());
  remote_endpoint->SetProxying(/*in_node_name=*/actual_node_name, /*in_endpoint_name=*/accept_invitation_endpoint_name);

  printf("  Our `remote_endpoint` now recognizes its proxy target as: (%s:%s)\n",
         remote_endpoint->proxy_target.node_name.c_str(),
         remote_endpoint->proxy_target.endpoint_name.c_str());

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
  //       the remote node's endpoint gets them. Note that the messages queued
  //       in `remote_endpoint` might be carrying `EndpointDescriptor`s, each of
  //       which we have to examine so we can:
  //         1.) Take all of the queued messages in the endpoint described by
  //             the info, and forward them to the remote node (is it guaranteed
  //             to be remote?)
  //         2.) Set the endpoint described by the info to the proxying mode (or
  //             maybe just delete it? Figure this out....)
  std::queue<Message> messages_to_forward =
      remote_endpoint->TakeQueuedMessages();
  printf("    Node has %lu messages queued up in the remote invitation endpoint\n", messages_to_forward.size());

  // TODO(domfarolino): I think this is just a stupid artifact of the fact that
  // we let the recipient of the invitation choose its own name. We should still
  // choose the intended endpoint name for it, but just make it a random name
  // instead. That way we can avoid doing this update.
  //
  // All of these messages were written with the `target_endpoint` as
  // `remote_endpoint->name`. But since `remote_endpoint` is now proxying to a
  // different-named endpoint in the remote process, we must re-target the
  // queued messages. Re-targeting of the dependant messages happens in
  // `SendMessagesAndRecursiveDependants()`.
  std::queue<Message> final_messages_to_forward;
  while (!messages_to_forward.empty()) {
    Message message_to_forward = std::move(messages_to_forward.front());
    memcpy(message_to_forward.GetMutableMessageHeader().target_endpoint, remote_endpoint->proxy_target.endpoint_name.c_str(), kIdentifierSize);
    final_messages_to_forward.push(std::move(message_to_forward));
    messages_to_forward.pop();
  }

  SendMessagesAndRecursiveDependants(std::move(final_messages_to_forward), remote_endpoint);
  Core::Get()->OnReceivedAcceptInvitation();
}

void Node::SendMessagesAndRecursiveDependants(std::queue<Message> messages_to_send, std::shared_ptr<Endpoint> local_peer_endpoint) {
  // All messages sent from this method are bound for the same node. But each
  // message is going to an endpoint of a different name in the remote node.
  // The name of the cross-node endpoint is captured in
  // `EndpointDescriptor::cross_node_endpoint_name`, which is what we'll:
  //   1.) Set `MessageHeader::target_endpoint` to
  //   2.) Set the backing endpoint's proxy target's endpoint name to
  std::string target_node_name = local_peer_endpoint->proxy_target.node_name;

  while (!messages_to_send.empty()) {
    Message message_to_send = std::move(messages_to_send.front());

    // Push possibly many more messages to `message_to_send`.
    printf("      Forwarding a message NumberOfHandles(): %d\n", message_to_send.NumberOfHandles());
    std::vector<EndpointDescriptor> descriptors = message_to_send.GetEndpointDescriptors();
    for (const EndpointDescriptor& descriptor : descriptors) {
      std::string endpoint_name(descriptor.endpoint_name, 15);
      std::string cross_node_endpoint_name(descriptor.cross_node_endpoint_name, 15);
      printf("        An EndpointDescriptor in this message:\n");
      descriptor.Print();

      auto it = local_endpoints_.find(endpoint_name);
      CHECK_NE(it, local_endpoints_.end());

      std::shared_ptr<Endpoint> endpoint_from_info = it->second;
      std::queue<Message> sub_messages = endpoint_from_info->TakeQueuedMessages();
      while (!sub_messages.empty()) {
        Message sub_message = std::move(sub_messages.front());
        // See
        // `Core::PopulateEndpointDescriptorAndMaybeSetEndpointInProxyingState()`.
        // When we create an `EndpointDescriptor`, we create it with a new name
        // that the target endpoint will get *if* the descriptor goes to another
        // process. Since we know `descriptor` is going to another process, we
        // have to take all of these messages queued on the endpoint
        // representing `descriptor` (i.e., `endpoint_from_info`) and change
        // their target endpoint names to `endpoint_from_info`'s cross-process
        // name (i.e., the name of that endpoint but in a different process).
        memcpy(sub_message.GetMutableMessageHeader().target_endpoint, cross_node_endpoint_name.c_str(), kIdentifierSize);
        messages_to_send.push(std::move(sub_message));
        sub_messages.pop();
      }

      // We know that the real endpoint was sent to `target_node_name`. By now,
      // it is possible that endpoint was sent yet again to another process, and
      // so on. Its ultimate location doesn't matter to us. We know the next
      // place it went was `target_node_name` which is either the ultimate
      // destination, or the node with the next closest proxy. We'll send the
      // message to that node and let it figure out what to do from there.
      endpoint_from_info->SetProxying(/*in_node_name=*/target_node_name, /*in_endpoint_name=*/cross_node_endpoint_name);
    }

    // Forward the message and remove it from the queue.
    node_channel_map_[target_node_name]->SendMessage(std::move(message_to_send));
    messages_to_send.pop();
  }
}

void Node::OnReceivedUserMessage(Message message) {
  CHECK_ON_THREAD(base::ThreadType::IO);
  printf("Node::OnReceivedUserMessage getpid(): %d\n", getpid());
  // 1. Extract the endpoint that the message is bound for.
  char* target_endpoint_buffer = message.GetMutableMessageHeader().target_endpoint;
  std::string local_target_endpoint_name(
      target_endpoint_buffer,
      target_endpoint_buffer + kIdentifierSize);
  printf("    OnReceivedUserMessage() looking for local_target_endpoint_name: %s to dispatch message to\n", local_target_endpoint_name.c_str());
  auto endpoint_it = local_endpoints_.find(local_target_endpoint_name);
  CHECK_NE(endpoint_it, local_endpoints_.end());

  std::shared_ptr<Endpoint> endpoint = endpoint_it->second;
  CHECK(endpoint);
  CHECK_EQ(local_target_endpoint_name, endpoint->name);

  // 2. Tell the endpoint to handle the message.
  endpoint->AcceptMessageOnIOThread(std::move(message));
}

}; // namespace mage
