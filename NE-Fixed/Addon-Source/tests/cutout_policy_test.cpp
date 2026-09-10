// SPDX-License-Identifier: MIT
#include "CutoutPolicy.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using NEFixed::NeedsCutoutRepair;
int main() {
  assert(NeedsCutoutRepair(1, .999f));
  assert(NeedsCutoutRepair(0, .001f));
  assert(NeedsCutoutRepair(.5f, .502f)); // Keep exact authored partial dissolve.
  assert(!NeedsCutoutRepair(1, 1));
  assert(!NeedsCutoutRepair(.5f, .5f));
  assert(!NeedsCutoutRepair(.5f, .6f)); // Not this NE dead-zone failure.
  assert(!NeedsCutoutRepair(-.001f, 0));
  assert(!NeedsCutoutRepair(1.001f, 1));
  assert(!NeedsCutoutRepair(std::numeric_limits<float>::quiet_NaN(), 0));
  assert(!NeedsCutoutRepair(0, std::numeric_limits<float>::infinity()));
  unsigned originalCalls = 0, repairs = 0;
  float applied = 0;
  auto chain = [&](float request, bool addonEnabled) {
    ++originalCalls;
    // Model the reviewed NE 1.6.3 hook: skip <= .005, otherwise call game.
    if (std::fabs(request - applied) > .005f) applied = request;
    if (addonEnabled && NeedsCutoutRepair(request, applied)) { applied = request; ++repairs; }
  };
  chain(.999f, true);
  chain(1, true);
  assert(applied == 1 && repairs == 1);
  chain(.5f, true);
  chain(.501f, true);
  assert(applied == .501f && repairs == 2);
  chain(.001f, true);
  chain(0, true);
  assert(applied == 0 && repairs == 3);
  chain(0, true);
  assert(repairs == 3); // No repeated work on an unchanged value.
  chain(.001f, false);
  assert(applied == 0 && repairs == 3 && originalCalls == 8);
  std::cout << "Cutout precision, exact endpoints, preserved partial values and original chain policy passed\n";
}
