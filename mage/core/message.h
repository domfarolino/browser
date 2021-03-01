#ifndef MAGE_CORE_MESSAGE_H_
#define MAGE_CORE_MESSAGE_H_

#include <string>
#include <vector>

#include "base/check.h"

namespace mage {

static const int kInvalidFragmentStartingIndex = -1;

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

struct MessageHeader {
  int size;
  MessageType type;
};

class Message final {
 public:
  Message(MessageType type) : type_(type) {
    int num_bytes_to_allocate = sizeof(MessageHeader);
    payload_buffer_.resize(num_bytes_to_allocate + payload_buffer_.size());
    auto header = Get<MessageHeader>(0);
    // We'll write the final size in |FinalizeSize()|.
    header->type = type_;
  }

  template <typename MessageFragment>
  MessageFragment* Get(int starting_index) {
    return reinterpret_cast<MessageFragment*>(payload_buffer_.data() +
                                              starting_index);
  }

  // Must be called before actually sending the message.
  void FinalizeSize() {
    Get<MessageHeader>(/*starting_index=*/0)->size = payload_buffer_.size();
  }

  void TakeBuffer(std::vector<char>& incoming_buffer) {
    payload_buffer_ = std::move(incoming_buffer);
  }

  MessageType Type() {
    return type_;
  }

  std::vector<char>& payload_buffer() {
    return payload_buffer_;
  }

  // Serializes the given message type
  static std::unique_ptr<Message> Deserialize(int fd) {
    NOTREACHED();
  }

 private:
  MessageType type_;
  std::vector<char> payload_buffer_;
};

template <typename T>
class MessageFragment {
 public:
  MessageFragment(Message& message) : message_(message), starting_index_(kInvalidFragmentStartingIndex) {}

  void Allocate() {
    // Cache the starting index of the block of data that we'll allocate. Note
    // that this index is invalid at this moment, but right when the buffer is
    // resized to make room for however many bytes |T| takes up, then this index
    // will point to the first byte of our freshly-allocated block.
    starting_index_ = message_.payload_buffer().size();

    // Allocate enough bytes in the underlying message buffer for T.
    int num_bytes_to_allocate = sizeof(T);
    message_.payload_buffer().resize(message_.payload_buffer().size() + num_bytes_to_allocate);
  }

  T* data() {
    // Must only be invoked after |Allocate()|.
    return message_.Get<T>(starting_index_);
  }

 private:
  Message& message_;
  int starting_index_;
};

template <typename T>
struct ArrayHeader {
  T* array_storage() {
    // Take our address, and jump ahead "1" of "us", which is really jumping
    // ahead |sizeof(*this)| bytes. This will get us to the very first byte
    // after |this|, which is where the array data is actually stored.
    return reinterpret_cast<T*>(this + 1);
  }

  int num_elements;
};

template <typename T>
class MessageFragment<ArrayHeader<T>> {
 public:
  MessageFragment(Message& message) : message_(message), starting_index_(kInvalidFragmentStartingIndex) {}

  void AllocateArray(int num_elements) {
    // See comment in |MessageFragment<T>::Allocate()|.
    starting_index_ = message_.payload_buffer().size();

    // Allocate enough bytes for the ArrayHeader + the actual array data.
    int num_bytes_to_allocate = sizeof(ArrayHeader<T>) + (sizeof(T) * num_elements);
    message_.payload_buffer().resize(message_.payload_buffer().size() + num_bytes_to_allocate);

    // Get a pointer to the ArrayHeader<T>* that we just allocated above, and
    // set its |num_elements| member. This writes that number to the underlying
    // message buffer where the |ArrayHeader<T>| is allocated, so we can recover
    // it later.
    ArrayHeader<T>* array_header = data();
    array_header->num_elements = num_elements;
  }

  // Gets a pointer to the first byte of data that we allocated. Note that above
  // we allocated enough bytes for the array header as well as the actual array
  // data, so when we get a pointer to the first byte that we allocated, our
  // intention is to reference the ArrayHeader at that place. If you want to
  // access the actual array *storage* (which starts at the first byte after the
  // ArrayHeader), then you should use |ArrayHeader::array_storage()|.
  ArrayHeader<T>* data() {
    return message_.Get<ArrayHeader<T>>(starting_index_);
  }

 private:
  Message& message_;
  int starting_index_;
};

// The pointer helpers below were pulled from Chromium.
// Pointers are encoded as relative offsets. The offsets are relative to the
// address of where the offset value is stored, such that the pointer may be
// recovered with the expression:
//
//   ptr = reinterpret_cast<char*>(offset) + *offset
//
// A null pointer is encoded as an offset value of 0.
//
inline void EncodePointer(const void* ptr, uint64_t* offset) {
  if (!ptr) {
    *offset = 0;
    return;
  }

  const char* p_obj = reinterpret_cast<const char*>(ptr);
  const char* p_slot = reinterpret_cast<const char*>(offset);
  CHECK(p_obj > p_slot);

  *offset = static_cast<uint64_t>(p_obj - p_slot);
}

// Note: This function doesn't validate the encoded pointer value.
inline const void* DecodePointer(const uint64_t* offset) {
  if (!*offset)
    return nullptr;
  return reinterpret_cast<const char*>(offset) + *offset;
}

template <typename T>
struct Pointer {
  void Set(T* ptr) { EncodePointer(ptr, &offset); }
  const T* Get() const { return static_cast<const T*>(DecodePointer(&offset)); }
  T* Get() {
    return static_cast<T*>(const_cast<void*>(DecodePointer(&offset)));
  }

  uint64_t offset = 0;
};

struct SendInvitationParams {
  Pointer<ArrayHeader<char>> inviter_name;
  Pointer<ArrayHeader<char>> temporary_remote_node_name;
  Pointer<ArrayHeader<char>> intended_endpoint_name;
  Pointer<ArrayHeader<char>> intended_endpoint_peer_name;
};

struct SendAcceptInvitationParams {
  Pointer<ArrayHeader<char>> temporary_remote_node_name;
  Pointer<ArrayHeader<char>> actual_node_name;
};

}; // namspace mage

#endif // MAGE_CORE_MESSAGE_H_
