// SPDX-License-Identifier: MIT
#include "AddonPolicy.hpp"
#include <cassert>
#include <iostream>
using namespace NEFixed;
int main() {
  assert(CheckBase(true, "NoodleExtensions", "1.6.3") == BaseStatus::Ready);
  assert(CheckBase(false, "NoodleExtensions", "1.6.3") == BaseStatus::MissingOrFailed);
  for (auto version : {"", "1.6.2", "1.6.4", "1.8.15", "1.6.3-fork"})
    assert(CheckBase(true, "NoodleExtensions", version) == BaseStatus::Unsupported);
  assert(CheckBase(true, "ne-fixed", "1.6.3") == BaseStatus::Unsupported);
  assert(CheckBase(true, "noodleextensions", "1.6.3") == BaseStatus::Unsupported);
  for (auto status : {BaseStatus::Ready, BaseStatus::MissingOrFailed, BaseStatus::Unsupported})
    for (bool enabled : {false, true})
      for (bool defaults : {false, true})
        for (bool effects : {false, true})
          assert(ShouldApply(status, enabled, defaults, effects) ==
                 (status == BaseStatus::Ready && enabled && defaults && effects));
  std::cout << "Add-on base-version and patch ownership policy passed\n";
}
