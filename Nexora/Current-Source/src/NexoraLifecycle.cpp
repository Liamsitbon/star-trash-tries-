#include "NexoraLifecycle.hpp"

#include <limits>

namespace Nexora {

void RuntimeLifecycle::AdvanceGeneration() noexcept {
  _generation = _generation == std::numeric_limits<std::uint64_t>::max() ? 1 : _generation + 1;
}

std::uint64_t RuntimeLifecycle::BeginPreparation() noexcept {
  AdvanceGeneration();
  _phase = RuntimePhase::Preparing;
  return _generation;
}

bool RuntimeLifecycle::Activate(std::uint64_t generation) noexcept {
  if (_phase != RuntimePhase::Preparing || generation != _generation) return false;
  _phase = RuntimePhase::Active;
  return true;
}

void RuntimeLifecycle::Suspend() noexcept {
  if (_phase == RuntimePhase::Active) _phase = RuntimePhase::Suspended;
}

void RuntimeLifecycle::Resume() noexcept {
  if (_phase == RuntimePhase::Suspended) _phase = RuntimePhase::Active;
}

std::uint64_t RuntimeLifecycle::BeginRetirement() noexcept {
  if (_phase != RuntimePhase::Retiring) {
    AdvanceGeneration();
    _phase = RuntimePhase::Retiring;
  }
  return _generation;
}

bool RuntimeLifecycle::CompleteRetirement() noexcept {
  if (_phase != RuntimePhase::Retiring || _renderDepth != 0) return false;
  _phase = RuntimePhase::Dormant;
  return true;
}

bool RuntimeLifecycle::CanRender(std::uint64_t generation) const noexcept {
  return _phase == RuntimePhase::Active && generation == _generation;
}

bool RuntimeLifecycle::TryEnterRender(std::uint64_t generation) noexcept {
  if (!CanRender(generation)) return false;
  ++_renderDepth;
  return true;
}

void RuntimeLifecycle::LeaveRender() noexcept {
  if (_renderDepth != 0) --_renderDepth;
}

}  // namespace Nexora

