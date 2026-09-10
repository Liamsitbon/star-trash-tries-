#pragma once

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Nexora {
// Only the local user interface can create these bindings. Authored map JSON
// never reaches the external-path resolver. Bindings are session-local, keyed
// by the game's exact level ID, and do not modify the map or move media.
struct ExternalVideo {
  std::string levelId;
  std::filesystem::path path;
  float offsetSeconds = 0;
  float yawDegrees = 0;
};

class ExternalSelection {
 public:
  void Bind(ExternalVideo video);
  void Clear() { _video.reset(); }
  ExternalVideo const* ForLevel(std::string_view levelId) const;
  ExternalVideo const* Current() const { return _video ? &*_video : nullptr; }
 private:
  std::optional<ExternalVideo> _video;
};

struct MediaEntry { std::filesystem::path path; bool directory = false; };
struct MediaDirectory {
  std::filesystem::path path;
  std::vector<MediaEntry> entries;
  bool truncated = false;
};

bool IsVideoExtension(std::filesystem::path const& path);
std::filesystem::path ReadableExternalVideo(std::filesystem::path const& path);
MediaDirectory BrowseMedia(std::filesystem::path const& directory,
                           std::size_t maximumEntries = 4096,
                           std::atomic_bool const* cancelled = nullptr);
// Explicit user action only: exclusive creation, no overwrite, source retained.
// Partial output is removed on error/cancel; no DAT or Info.dat is rewritten.
std::filesystem::path CopyVideoToMap(std::filesystem::path const& source,
                                   std::filesystem::path const& mapRoot,
                                   std::atomic_bool const& cancelled);
}  // namespace Nexora
