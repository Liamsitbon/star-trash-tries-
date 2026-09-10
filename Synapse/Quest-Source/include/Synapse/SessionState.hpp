#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace Synapse::Quest {
// Seconds throughout. One outstanding ping; no shared cancellation owner.
class ServerClock {
  std::array<double,30> offsets_{};
  std::size_t size_=0, next_=0;
  std::optional<float> pending_;
public:
  void Reset() { size_=next_=0; pending_.reset(); }
  bool BeginPing(float now) {
    if(pending_ || !std::isfinite(now) || now<0) return false;
    pending_=now; return true;
  }
  bool Pong(float echoed, float server, double received) {
    if(!pending_ || echoed!=*pending_ || !std::isfinite(server) || !std::isfinite(received)) return false;
    double rtt=received-echoed;
    if(rtt<0 || rtt>=10) { pending_.reset(); return false; }
    offsets_[next_]=server-received+rtt*.5;
    next_=(next_+1)%offsets_.size(); if(size_<offsets_.size()) ++size_;
    pending_.reset(); return true;
  }
  bool TimedOut(double now) {
    if(pending_ && std::isfinite(now) && now-*pending_>=10) { pending_.reset(); return true; }
    return false;
  }
  std::optional<double> Time(double now) const {
    if(!size_ || !std::isfinite(now)) return {};
    double offset=0; for(std::size_t i=0;i<size_;++i) offset+=offsets_[i];
    return now+offset/size_;
  }
};

// A map is not ready merely because a previous operation finished. The exact
// artifact and session generation must still match. No Unity references here.
class PreparedMapGate {
  std::uint64_t generation_=0;
  std::string artifact_;
  bool ready_=false;
public:
  std::uint64_t Begin(std::string artifact) {
    ++generation_; artifact_=std::move(artifact); ready_=false; return generation_;
  }
  void Cancel() { ++generation_; ready_=false; artifact_.clear(); }
  bool Complete(std::uint64_t generation,std::string const& artifact,bool verified) {
    if(generation!=generation_ || artifact!=artifact_ || artifact.empty() || !verified) return false;
    ready_=true; return true;
  }
  bool CanTransition() const { return ready_; }
};
} // namespace Synapse::Quest
