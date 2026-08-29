#include "VivifyQuestPerformance.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <set>

int main() {
  using Vivify::MeaningfulRateChange;
  using Vivify::PrefabInitialElapsed;
  using Vivify::RenderCommandCacheGate;
  using Vivify::StableSyncRate;
  using Vivify::StaggeredRefreshDelay;

  RenderCommandCacheGate cache;
  assert(!cache.Valid());
  assert(!cache.Matches(4, 0x1000, 0x55));
  cache.Commit(4, 0x1000, 0x55);
  assert(cache.Valid());
  assert(cache.Matches(4, 0x1000, 0x55));
  assert(!cache.Matches(5, 0x1000, 0x55));
  assert(!cache.Matches(4, 0x2000, 0x55));
  assert(!cache.Matches(4, 0x1000, 0x56));
  cache.Invalidate();
  assert(!cache.Valid());

  std::set<int> refreshDelays;
  for (std::uintptr_t token = 1; token <= 512; ++token) {
    int const delay = StaggeredRefreshDelay(token << 4);
    assert(delay >= 45);
    assert(delay <= 75);
    assert(delay == StaggeredRefreshDelay(token << 4));
    refreshDelays.emplace(delay);
  }
  // A broken mixer that schedules every root on the same frame defeats the
  // purpose of the policy. Exercise enough tokens to require broad spreading.
  assert(refreshDelays.size() >= 24);

  float const nan = std::numeric_limits<float>::quiet_NaN();
  assert(MeaningfulRateChange(nan, 1.0f));
  assert(!MeaningfulRateChange(1.0f, 1.0f));
  assert(!MeaningfulRateChange(1.0f, 1.0001f));
  assert(MeaningfulRateChange(1.0f, 0.75f));
  assert(MeaningfulRateChange(0.75f, 0.0f));

  assert(StableSyncRate(false, -1.0f, 0.0f, 1.0f) == 1.0f);
  assert(StableSyncRate(false, 10.0f, 10.011f, 0.8f) == 0.8f);
  assert(StableSyncRate(true, 10.0f, 10.011f, 0.8f) == 0.0f);
  assert(StableSyncRate(false, 10.0f, 10.0f, 1.0f) == 0.0f);
  assert(StableSyncRate(false, 10.0f, 9.0f, 1.0f) == 0.0f);
  assert(StableSyncRate(false, 10.0f, 10.011f, nan) == 0.0f);

  // Normal playback must start a newly fired prefab at its own time zero. The
  // old Quest path passed the absolute event time (for example 31.862s) into
  // Animator.Update and skipped the opening of Aether's drop animation.
  assert(PrefabInitialElapsed(31.862f, 31.862f) == 0.0f);
  assert(PrefabInitialElapsed(31.80f, 31.862f) == 0.0f);
  // Practice/catch-up starts at the elapsed position inside the event.
  assert(std::fabs(PrefabInitialElapsed(35.162f, 31.862f) - 3.3f) < 0.0001f);
  assert(PrefabInitialElapsed(nan, 31.862f) == 0.0f);
  assert(PrefabInitialElapsed(35.162f, nan) == 0.0f);

  return 0;
}
