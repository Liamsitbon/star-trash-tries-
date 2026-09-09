#pragma once

namespace Nexora {

// 0.3.5 ships the direct fragment path confirmed visible on Quest. A new key
// is intentional: an existing rawVideoDiagnostic=false from 0.3.4 must not
// prevent upgrades from receiving the release's default behavior.
inline constexpr char kDirectVideoSetting[] = "directVideoRendering";
inline constexpr bool kDefaultDirectVideoRendering = true;

constexpr bool UseDirectVideoRendering(bool directVideoRendering,
                                      bool rawVideoDiagnostic) noexcept {
  return directVideoRendering || rawVideoDiagnostic;
}

}  // namespace Nexora
