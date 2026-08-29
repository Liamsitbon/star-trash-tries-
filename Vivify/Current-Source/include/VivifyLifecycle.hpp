#pragma once

#include <cstdint>

namespace Vivify {

// RuntimePhase is deliberately independent from Unity.  The Quest runtime is
// persistent, while every gameplay camera and asset graph is scene-owned; an
// explicit phase and generation prevent a callback from an old scene from
// observing a newly prepared beatmap.
enum class RuntimePhase : std::uint8_t {
  Dormant,
  Preparing,
  Active,
  Suspended,
  Retiring,
};

class RuntimeLifecycle final {
public:
  [[nodiscard]] std::uint64_t BeginPreparation() noexcept;
  [[nodiscard]] bool Activate(std::uint64_t generation) noexcept;

  void Suspend() noexcept;
  void Resume() noexcept;

  // Invalidates all scene-owned clients immediately.  Calling this more than
  // once during the same retirement is idempotent.
  [[nodiscard]] std::uint64_t BeginRetirement() noexcept;
  [[nodiscard]] bool CompleteRetirement() noexcept;

  [[nodiscard]] bool CanRender(std::uint64_t generation) const noexcept;
  [[nodiscard]] bool TryEnterRender(std::uint64_t generation) noexcept;
  void LeaveRender() noexcept;

  [[nodiscard]] RuntimePhase Phase() const noexcept { return _phase; }
  [[nodiscard]] std::uint64_t Generation() const noexcept { return _generation; }
  [[nodiscard]] std::uint32_t RenderDepth() const noexcept { return _renderDepth; }
  [[nodiscard]] bool IsActive() const noexcept { return _phase == RuntimePhase::Active; }
  [[nodiscard]] bool IsRetiring() const noexcept { return _phase == RuntimePhase::Retiring; }

private:
  void AdvanceGeneration() noexcept;

  RuntimePhase _phase = RuntimePhase::Dormant;
  std::uint64_t _generation = 1;
  std::uint32_t _renderDepth = 0;
};

}  // namespace Vivify
