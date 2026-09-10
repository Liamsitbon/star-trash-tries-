// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>

namespace NEFixed {
// Called AFTER the original hook chain. Repair only an unapplied finite,
// in-range change inside the official 1.6.3 hook's dead zone. Never force
// authored partial visibility to an endpoint or mask a large discrepancy.
inline bool NeedsCutoutRepair(float requested, float applied) {
  return std::isfinite(requested) && std::isfinite(applied) &&
    requested >= 0 && requested <= 1 && applied >= 0 && applied <= 1 &&
    requested != applied && std::fabs(requested - applied) <= 0.005f;
}
}  // namespace NEFixed
