#pragma once

#include <cstdint>

namespace Nexora {

enum class RuntimePhase : std::uint8_t { Dormant, Preparing, Active, Suspended, Retiring };

// Adapted from Vivify's MIT-licensed generation-gated scene lifecycle.  A
// persistent Quest behaviour may receive callbacks after a gameplay scene has
// started retiring, so every render client is tied to one generation.
class RuntimeLifecycle final {
public:
  [[nodiscard]] std::uint64_t BeginPreparation() noexcept;
  [[nodiscard]] bool Activate(std::uint64_t generation) noexcept;
  void Suspend() noexcept;
  void Resume() noexcept;
  [[nodiscard]] std::uint64_t BeginRetirement() noexcept;
  [[nodiscard]] bool CompleteRetirement() noexcept;
  [[nodiscard]] bool CanRender(std::uint64_t generation) const noexcept;
  [[nodiscard]] bool TryEnterRender(std::uint64_t generation) noexcept;
  void LeaveRender() noexcept;
  [[nodiscard]] bool IsDormant() const noexcept { return _phase == RuntimePhase::Dormant; }
  [[nodiscard]] bool IsActive() const noexcept { return _phase == RuntimePhase::Active; }
  [[nodiscard]] bool IsSuspended() const noexcept { return _phase == RuntimePhase::Suspended; }
  [[nodiscard]] bool IsRetiring() const noexcept { return _phase == RuntimePhase::Retiring; }
  [[nodiscard]] std::uint64_t Generation() const noexcept { return _generation; }
  [[nodiscard]] std::uint32_t RenderDepth() const noexcept { return _renderDepth; }

private:
  void AdvanceGeneration() noexcept;
  RuntimePhase _phase = RuntimePhase::Dormant;
  std::uint64_t _generation = 1;
  std::uint32_t _renderDepth = 0;
};

}  // namespace Nexora
