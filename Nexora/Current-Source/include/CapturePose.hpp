#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Nexora {
// Media seconds, capture-space metres, normalized XYZW quaternion. The map's
// authored root transform is applied AFTER this pose, never in place of depth.
struct CapturePose {
  double time = 0;
  std::array<float, 3> position{};
  std::array<float, 4> rotation{0, 0, 0, 1};
};
inline bool ValidCapturePoses(std::vector<CapturePose> const& poses) {
  if (poses.size() > 16384) return false;
  double previous = -1;
  for (auto const& p : poses) {
    if (!std::isfinite(p.time) || p.time < 0 || p.time <= previous) return false;
    previous = p.time;
    float norm = 0;
    for (float v : p.position) if (!std::isfinite(v) || std::abs(v) > 1000) return false;
    for (float v : p.rotation) { if (!std::isfinite(v)) return false; norm += v*v; }
    if (std::abs(norm - 1) > 0.01f) return false;
  }
  return true;
}
inline CapturePose SampleCapturePose(std::vector<CapturePose> const& poses, double time) {
  if (poses.empty() || !std::isfinite(time)) return {};
  auto next = std::upper_bound(poses.begin(), poses.end(), time,
      [](double t, CapturePose const& p) { return t < p.time; });
  if (next == poses.begin()) return poses.front();
  if (next == poses.end()) return poses.back();
  auto const& a = *(next-1); auto const& b = *next;
  float t = static_cast<float>((time - a.time) / (b.time - a.time));
  CapturePose out; out.time = time;
  for (int i=0; i<3; ++i) out.position[i] = a.position[i] + (b.position[i]-a.position[i])*t;
  float dot=0, norm=0;
  for (int i=0; i<4; ++i) dot += a.rotation[i]*b.rotation[i];
  // Shortest-path normalized lerp: bounded, deterministic and seek independent.
  for (int i=0; i<4; ++i) {
    out.rotation[i] = a.rotation[i]*(1-t) + b.rotation[i]*t*(dot < 0 ? -1 : 1);
    norm += out.rotation[i]*out.rotation[i];
  }
  for (float& v : out.rotation) v /= std::sqrt(norm);
  return out;
}
} // namespace Nexora
