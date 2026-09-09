#pragma once

#include <cmath>
#include <algorithm>

namespace Nexora {

// One frame: a mono equirectangular RGB panel on the left and a matching
// radial-distance panel on the right. The depth panel may be lower resolution
// horizontally; both panels cover the entire capture, not separate eyes.
struct RgbdVideo {
  bool enabled = false;
  float nearMeters = 2.0f;
  float farMeters = 100.0f;
  float colorWidth = 0.8f;
  float strength = 1.0f;
  float worldScale = 1.0f;
  int quality = 1;

  bool IsValid() const {
    return std::isfinite(nearMeters) && std::isfinite(farMeters) &&
           std::isfinite(colorWidth) && nearMeters >= 2.0f &&
           farMeters > nearMeters && farMeters <= 500.0f &&
           colorWidth >= 0.5f && colorWidth <= 0.9f &&
           std::isfinite(strength) && strength >= 0 && strength <= 1 &&
           std::isfinite(worldScale) && worldScale >= 0.1f && worldScale <= 4 &&
           nearMeters * worldScale >= 0.5f && farMeters * worldScale <= 500 &&
           quality >= 0 && quality <= 2;
  }
  int Rings() const { return quality == 0 ? 48 : quality == 1 ? 72 : 96; }
  int Segments() const { return Rings() * 2; }
  float Distance(float code) const {
    return (nearMeters + (farMeters - nearMeters) * std::clamp(code, 0.0f, 1.0f)) * worldScale;
  }
};

inline constexpr int kRgbdRings = 96;
inline constexpr int kRgbdSegments = 192;

}  // namespace Nexora
