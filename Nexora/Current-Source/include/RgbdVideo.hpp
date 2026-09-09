#pragma once

#include <cmath>

namespace Nexora {

// One frame: a mono equirectangular RGB panel on the left and a matching
// radial-distance panel on the right. The depth panel may be lower resolution
// horizontally; both panels cover the entire capture, not separate eyes.
struct RgbdVideo {
  bool enabled = false;
  float nearMeters = 2.0f;
  float farMeters = 100.0f;
  float colorWidth = 0.8f;

  bool IsValid() const {
    return std::isfinite(nearMeters) && std::isfinite(farMeters) &&
           std::isfinite(colorWidth) && nearMeters >= 2.0f &&
           farMeters > nearMeters && farMeters <= 500.0f &&
           colorWidth >= 0.5f && colorWidth <= 0.9f;
  }
};

inline constexpr int kRgbdRings = 96;
inline constexpr int kRgbdSegments = 192;

}  // namespace Nexora
