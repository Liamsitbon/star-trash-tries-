// https://github.com/Kautenja/object-pool/blob/master/include/object_pool.hpp

#pragma once

#include <deque>
#include <unordered_set>
#include <vector>
#include "Animation/NoodleMovementDataProvider.hpp"
#include "GlobalNamespace/BeatmapObjectData.hpp"
#include "NELogger.h"

namespace NoodleExtensions::Pool {

class NoodleMovementDataProviderPool {
private:
  std::deque<SafePtr<NoodleMovementDataProvider>> free = {};
  std::unordered_set<NoodleMovementDataProvider*> pooled = {};
  // Keep every leased provider rooted as well as the free providers. Dense
  // fake-note effects (Murder Plot leases 251 at once) exceed the original
  // pool of 75, and a freshly allocated provider otherwise has no native root
  // after the Init hook returns.
  std::vector<SafePtr<NoodleMovementDataProvider>> owned = {};

public:
  NoodleMovementDataProviderPool(int count) : free() {
    for (int i = 0; i < count; ++i) {
      SafePtr<NoodleMovementDataProvider> provider(
          NoodleMovementDataProvider::New_ctor());
      owned.emplace_back(provider);
      put(provider);
    }
  }

  SafePtr<NoodleMovementDataProvider> get(GlobalNamespace::BeatmapObjectData* beatmapObjectData) {
    SafePtr<NoodleMovementDataProvider> obj;
    if (!free.empty()) {
      obj.emplace(free.back().ptr());
      free.pop_back();
      pooled.erase(obj.ptr());
    }

    if (!obj) {
      obj.emplace(NoodleMovementDataProvider::New_ctor());
      owned.emplace_back(obj);
    }
    obj->InitObject(beatmapObjectData);
    return obj;
  }

  void put(SafePtr<NoodleMovementDataProvider> obj) {
    if (!obj) return;

    auto* raw = obj.ptr();
    if (!pooled.emplace(raw).second) {
      NELogger::Logger.warn("Ignored a duplicate NoodleMovementDataProvider pool return");
      return;
    }

    // Clear per-object overrides before the provider becomes visible to another
    // controller. This also makes an accidental late read deterministic.
    obj->InitObject(nullptr);
    free.emplace_back(obj);
  }
};

} // namespace NoodleExtensions::Pool
