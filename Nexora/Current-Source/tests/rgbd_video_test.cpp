#include "RgbdVideo.hpp"

#include <cassert>
#include <limits>

int main() {
  using Nexora::RgbdVideo;
  RgbdVideo defaults;
  assert(!defaults.enabled && defaults.IsValid());
  assert((RgbdVideo{true, 2, 100, 0.8f}.IsValid()));
  assert((RgbdVideo{true, 2, 500, 0.5f}.IsValid()));
  assert(!(RgbdVideo{true, 0, 100, 0.8f}.IsValid()));
  assert(!(RgbdVideo{true, 100, 2, 0.8f}.IsValid()));
  assert(!(RgbdVideo{true, 100, 100, 0.8f}.IsValid()));
  assert(!(RgbdVideo{true, 2, 501, 0.8f}.IsValid()));
  assert(!(RgbdVideo{true, 2, 100, 1}.IsValid()));
  assert(!(RgbdVideo{true, 2, 100, std::numeric_limits<float>::quiet_NaN()}.IsValid()));
  assert(!(RgbdVideo{true, 2, std::numeric_limits<float>::infinity(), 0.8f}.IsValid()));
  static_assert((Nexora::kRgbdRings + 1) * (Nexora::kRgbdSegments + 1) < 65536);
}
