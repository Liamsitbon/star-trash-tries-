#pragma once
#include "Protocol.hpp"
#include <mutex>
#include <optional>
#include <utility>
namespace Synapse::Quest {
// Worker/main-thread boundary. No Unity objects and no borrowed packet spans.
// Overflow is reported to the future transport as a disconnect condition, not
// silently dropped gameplay/control messages. Old sessions cannot enqueue.
class MessageQueue {
  static constexpr std::size_t Capacity=64;
  std::array<std::optional<ServerMessage>,Capacity> messages_;
  std::mutex mutex_;
  std::uint64_t generation_=0;
  std::size_t head_=0,size_=0;
public:
  std::uint64_t NewSession() {
    std::lock_guard lock(mutex_);
    for(auto& message:messages_) message.reset();
    head_=size_=0;
    return ++generation_;
  }
  bool Push(std::uint64_t generation, ServerMessage message) {
    std::lock_guard lock(mutex_);
    if(generation!=generation_ || size_==Capacity) return false;
    messages_[(head_+size_)%Capacity]=std::move(message);
    ++size_;
    return true;
  }
  std::optional<ServerMessage> Pop() {
    std::lock_guard lock(mutex_);
    if(!size_) return {};
    auto message=std::move(messages_[head_]);
    messages_[head_].reset(); head_=(head_+1)%Capacity; --size_;
    return message;
  }
};
}
