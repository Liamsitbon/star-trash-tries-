#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Vivify {

// Pure, host-testable policy for retaining Unity command buffers. The runtime
// commits a key only after buffers were installed successfully. A generation,
// owner or render-graph change therefore forces one rebuild, while identical
// Quest frames reuse the already attached buffers.
class RenderCommandCacheGate {
public:
  bool Matches(std::uint64_t generation, std::uintptr_t owner,
               std::size_t signature) const noexcept {
    return _valid && _generation == generation && _owner == owner &&
           _signature == signature;
  }

  void Commit(std::uint64_t generation, std::uintptr_t owner,
              std::size_t signature) noexcept {
    _generation = generation;
    _owner = owner;
    _signature = signature;
    _valid = true;
  }

  void Invalidate() noexcept {
    _generation = 0;
    _owner = 0;
    _signature = 0;
    _valid = false;
  }

  bool Valid() const noexcept { return _valid; }

private:
  std::uint64_t _generation = 0;
  std::uintptr_t _owner = 0;
  std::size_t _signature = 0;
  bool _valid = false;
};

// All renderer roots are commonly discovered in the same frame. Giving each
// root a deterministic refresh offset prevents the old once-per-second burst
// where every GetComponentsInChildren scan landed on one Quest frame.
constexpr int StaggeredRefreshDelay(std::uintptr_t token,
                                    int minimumFrames = 45,
                                    int spreadFrames = 31) noexcept {
  if (minimumFrames < 1) minimumFrames = 1;
  if (spreadFrames < 1) return minimumFrames;
  std::uintptr_t mixed = token;
  mixed ^= mixed >> 17;
  mixed *= static_cast<std::uintptr_t>(0xed5ad4bbU);
  mixed ^= mixed >> 11;
  return minimumFrames + static_cast<int>(mixed % static_cast<std::uintptr_t>(spreadFrames));
}

inline bool MeaningfulRateChange(float previous, float current,
                                 float epsilon = 0.0005f) noexcept {
  return !std::isfinite(previous) || !std::isfinite(current) ||
         std::fabs(previous - current) > epsilon;
}

// AudioTimeSyncController::timeScale is stable at the authored practice rate;
// deriving the same value from song-time / frame-time deltas amplifies normal
// scheduler jitter and can trigger Animator/ParticleSystem native setters every
// frame. A non-advancing timeline is still treated as paused.
inline float StableSyncRate(bool pausedOrUnavailable, float previousSongTime,
                            float currentSongTime, float timeScale) noexcept {
  if (pausedOrUnavailable || !std::isfinite(currentSongTime) ||
      !std::isfinite(timeScale) || timeScale <= 0.0f) {
    return 0.0f;
  }
  if (previousSongTime >= 0.0f && currentSongTime <= previousSongTime) {
    return 0.0f;
  }
  return timeScale;
}

// InstantiatePrefab event times are absolute song times. Unity Animator.Update
// expects time elapsed inside the newly created prefab, not the event's
// position in the song. Keeping this policy pure makes the normal-play and
// practice/catch-up behavior independently testable on the host.
inline float PrefabInitialElapsed(float currentSongTime,
                                  float eventStartSongTime) noexcept {
  if (!std::isfinite(currentSongTime) ||
      !std::isfinite(eventStartSongTime)) {
    return 0.0f;
  }
  return std::fmax(0.0f, currentSongTime - eventStartSongTime);
}

}  // namespace Vivify
