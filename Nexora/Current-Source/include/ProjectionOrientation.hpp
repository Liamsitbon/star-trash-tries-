#pragma once
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace Nexora {
// The dome mesh maps U=0 to +Z and U=.5 to -Z. Conventional equirectangular
// videos have their forward view at the image centre, so they need a half turn.
// Keep this media convention separate from animated root yaw/capture poses.
inline float ProjectionCorrection(std::string_view convention, float initialYaw) {
  if (convention == "u0-forward") return 0;
  if (convention == "center-forward") return 180;
  if (!convention.empty()) throw std::runtime_error("Unknown video azimuthConvention");
  // Older maps sometimes already authored the exact 180-degree workaround.
  // Preserve that initial compensation, but do not re-evaluate during yaw FX.
  if (std::isfinite(initialYaw) && std::abs(std::abs(std::remainder(initialYaw, 360.0f)) - 180) < .01f)
    return 0;
  return 180;
}
}  // namespace Nexora
