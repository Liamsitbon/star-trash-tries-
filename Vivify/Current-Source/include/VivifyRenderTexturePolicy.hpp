#pragma once
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <optional>

namespace Vivify::RenderTexturePolicy {
// Keep the original format whenever supported. Fallbacks preserve the numeric
// domain and precision of the AUTHORED channels. Added channels can change a
// shader that reads channels absent from the original format; log every fallback
// and require a map/device test. This is not universal shader compatibility.
template<class Format, class Supports>
std::optional<Format> Resolve(Format requested, Supports supports) {
  if (supports(requested)) return requested;
  auto firstSupported = [&](std::initializer_list<Format> candidates) -> std::optional<Format> {
    for (auto candidate : candidates) if (supports(candidate)) return candidate;
    return std::nullopt;
  };
  if (requested == Format::RFloat) return firstSupported({Format::RGFloat, Format::ARGBFloat});
  if (requested == Format::RGFloat) return firstSupported({Format::ARGBFloat});
  if (requested == Format::RHalf) return firstSupported({Format::RGHalf, Format::ARGBHalf, Format::RFloat, Format::RGFloat, Format::ARGBFloat});
  if (requested == Format::RGHalf) return firstSupported({Format::ARGBHalf, Format::RGFloat, Format::ARGBFloat});
  if (requested == Format::ARGBHalf || requested == Format::DefaultHDR) return firstSupported({Format::ARGBHalf, Format::ARGBFloat});
  if (requested == Format::RGB111110Float) return firstSupported({Format::ARGBHalf, Format::ARGBFloat});
  if (requested == Format::RInt) return firstSupported({Format::RGInt, Format::ARGBInt});
  if (requested == Format::RGInt) return firstSupported({Format::ARGBInt});
  if (requested == Format::R16 || requested == Format::RG32) return firstSupported({Format::ARGB64});
  if (requested == Format::ARGB32) return firstSupported({Format::BGRA32});
  if (requested == Format::BGRA32 || requested == Format::Default || requested == Format::R8 ||
      requested == Format::RG16 || requested == Format::RGB565 || requested == Format::ARGB4444 ||
      requested == Format::ARGB1555) return firstSupported({Format::ARGB32, Format::BGRA32});
  // Depth/shadow samplers, uint16, XR-encoded color and 32-bit float/int formats
  // must not silently become an 8-bit color texture or change sampler type.
  return std::nullopt;
}

inline int ScaledDimension(int requested, float ratio, int maximum) {
  maximum = std::max(1, maximum);
  int base = std::clamp(requested, 1, maximum);
  if (!std::isfinite(ratio) || ratio <= 0.0f) ratio = 1.0f;
  // Clamp BEFORE conversion. Tiny positive ratios previously overflowed the
  // float-to-int conversion; std::clamp after an invalid cast is too late.
  double scaled = static_cast<double>(base) / static_cast<double>(ratio);
  if (scaled >= maximum) return maximum;
  if (scaled <= 1.0) return 1;
  return static_cast<int>(scaled);
}
} // namespace Vivify::RenderTexturePolicy
