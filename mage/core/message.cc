#include "mage/core/message.h"

#include <memory>
#include <vector>

#include "mage/core/endpoint.h"

namespace mage {

namespace {

// The pointer helpers below were pulled from Chromium.
// Pointers are encoded in byte buffers as relative offsets. The offsets are
// relative to the address of where the offset *value* is stored, such that the
// pointer may be recovered with the expression:
//
//   ptr = reinterpret_cast<char*>(offset) + *offset
//
// The above expression takes the byte address of where the actual offset
// variable lives, and adds (*offset) bytes to this address, which is where the
// actual value that the pointer points to lives in memory.
//
// A null pointer is encoded as an offset value of 0.

// This method takes the two parameters:
//   - |ptr_storage|: The void* address of where exactly we're storing the value
//     that we'd like to point to.
//   - |out_offset|: The address of where we're actually storing the offset.
//     This is analogous to a stack-allocated pointer variable, whereas the
//     |ptr_storage| is the elsewhere-allocated address of the variable-sized
//     value we're pointing to.
inline void EncodePointer(const void* ptr_storage, uint64_t* out_offset) {
  if (!ptr_storage) {
    *out_offset = 0;
    return;
  }

  const char* ptr_storage_location = reinterpret_cast<const char*>(ptr_storage);
  const char* offset_location = reinterpret_cast<const char*>(out_offset);
  CHECK(ptr_storage_location > offset_location);

  *out_offset = static_cast<uint64_t>(ptr_storage_location - offset_location);
}

// Note: This function doesn't validate the encoded pointer value.
inline const void* DecodePointer(const uint64_t* offset) {
  if (!*offset)
    return nullptr;

  const char* address_of_offset_placeholder = reinterpret_cast<const char*>(offset);
  return address_of_offset_placeholder + *offset;
}

}; // namespace

//////////////////// MESSAGE ////////////////////

Message::Message(MessageType type) {
  int num_bytes_to_allocate = sizeof(MessageHeader);
  payload_buffer_.resize(num_bytes_to_allocate + payload_buffer_.size());

  // Set the MessageHeader's type to |type|. We'll finalize the header's size
  // in |FinalizeSize()|.
  GetMutableMessageHeader().type = type;
}

Message::Message(Message&& other) {
  payload_buffer_ = std::move(other.payload_buffer_);
}

//////////////////// END MESSAGE ////////////////////

//////////////////// POINTER ////////////////////

template <typename T>
void Pointer<T>::Set(T* ptr) {
  EncodePointer(ptr, &offset);
}

template <typename T>
const T* Pointer<T>::Get() const {
  return static_cast<const T*>(DecodePointer(&offset));
}

template <typename T>
T* Pointer<T>::Get() {
  return static_cast<T*>(const_cast<void*>(DecodePointer(&offset)));
}

//////////////////// END POINTER ////////////////////


}; // namspace mage
