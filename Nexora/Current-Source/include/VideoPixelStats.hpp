#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Nexora {

// CPU-only summary of explicitly sampled pixels. This never decides whether a
// movie is valid or changes playback: a legitimate frame can be black/white.
struct VideoPixelStats {
  std::array<unsigned, 3> minimum{255, 255, 255};
  std::array<unsigned, 3> maximum{0, 0, 0};
  std::array<std::uint64_t, 3> sum{};
  std::size_t count = 0;

  void Add(unsigned red, unsigned green, unsigned blue) {
    std::array<unsigned, 3> const rgb{red, green, blue};
    for (std::size_t channel = 0; channel < rgb.size(); ++channel) {
      minimum[channel] = std::min(minimum[channel], rgb[channel]);
      maximum[channel] = std::max(maximum[channel], rgb[channel]);
      sum[channel] += rgb[channel];
    }
    ++count;
  }

  double Mean(std::size_t channel) const {
    return count == 0 ? 0.0 : static_cast<double>(sum[channel]) / count;
  }
};

}  // namespace Nexora
