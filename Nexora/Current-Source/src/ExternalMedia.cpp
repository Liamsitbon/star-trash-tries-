#include "ExternalMedia.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace Nexora {
namespace {
struct File {
  int fd = -1;
  explicit File(int descriptor) : fd(descriptor) {}
  ~File() { if (fd >= 0) ::close(fd); }
  File(File const&) = delete;
  File& operator=(File const&) = delete;
};
void Error(char const* operation) { throw std::system_error(errno, std::generic_category(), operation); }
void CheckPath(std::filesystem::path const& path) {
  auto text = path.string();
  if (!path.is_absolute() || text.size() > 4096 ||
      std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; }))
    throw std::runtime_error("Choose an absolute local path without control characters");
}
}  // namespace

void ExternalSelection::Bind(ExternalVideo video) {
  if (video.levelId.empty() || video.levelId.size() > 1024 ||
      !std::isfinite(video.offsetSeconds) || std::abs(video.offsetSeconds) > 3600 ||
      !std::isfinite(video.yawDegrees) || std::abs(video.yawDegrees) > 360)
    throw std::runtime_error("Invalid custom song/video selection");
  video.path = ReadableExternalVideo(video.path);
  _video = std::move(video);
}
ExternalVideo const* ExternalSelection::ForLevel(std::string_view levelId) const {
  return _video && !levelId.empty() && _video->levelId == levelId ? &*_video : nullptr;
}
bool IsVideoExtension(std::filesystem::path const& path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return extension == ".mp4" || extension == ".m4v" || extension == ".mov" || extension == ".webm";
}
std::filesystem::path ReadableExternalVideo(std::filesystem::path const& path) {
  CheckPath(path);
  auto canonical = std::filesystem::canonical(path);
  CheckPath(canonical);
  if (!IsVideoExtension(canonical) || !std::filesystem::is_regular_file(canonical) ||
      ::access(canonical.c_str(), R_OK) != 0 || std::filesystem::file_size(canonical) == 0)
    throw std::runtime_error("Video is missing, empty, unreadable or has an unsupported extension");
  return canonical;
}
MediaDirectory BrowseMedia(std::filesystem::path const& directory, std::size_t maximumEntries,
                          std::atomic_bool const* cancelled) {
  CheckPath(directory);
  MediaDirectory result;
  result.path = std::filesystem::canonical(directory);
  // Enumerate only the chosen directory, never recursively walk the headset.
  for (auto const& entry : std::filesystem::directory_iterator(result.path)) {
    if (cancelled && cancelled->load()) throw std::runtime_error("Folder listing cancelled");
    std::error_code error;
    bool dir = entry.is_directory(error);
    if (error || entry.path().filename().string().starts_with('.')) continue;
    if (!dir && (!IsVideoExtension(entry.path()) || !entry.is_regular_file(error) || error)) continue;
    if (result.entries.size() >= maximumEntries) { result.truncated = true; break; }
    result.entries.push_back({entry.path(), dir});
  }
  std::sort(result.entries.begin(), result.entries.end(), [](auto const& a, auto const& b) {
    if (a.directory != b.directory) return a.directory;
    return a.path.filename().string() < b.path.filename().string();
  });
  return result;
}

std::filesystem::path CopyVideoToMap(std::filesystem::path const& source,
                                   std::filesystem::path const& mapRoot,
                                   std::atomic_bool const& cancelled) {
  auto const src = ReadableExternalVideo(source);
  CheckPath(mapRoot);
  auto root = std::filesystem::canonical(mapRoot);
  if (!std::filesystem::is_directory(root) ||
      (!std::filesystem::is_regular_file(root / "Info.dat") &&
       !std::filesystem::is_regular_file(root / "info.dat")))
    throw std::runtime_error("Copy requires an installed custom map folder (not an OST/DLC song)");
  File input(::open(src.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (input.fd < 0) Error("Open source video");
  struct stat initial{};
  if (::fstat(input.fd, &initial) != 0) Error("Read source metadata");
  if (!S_ISREG(initial.st_mode) || initial.st_size <= 0) throw std::runtime_error("Invalid source video");
  File directory(::open(root.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY));
  if (directory.fd < 0) Error("Open destination map");
  if (cancelled.load()) throw std::runtime_error("Copy cancelled");
  // A distinct, visible name prevents collisions with a map's authored video.
  auto const name = "Nexora-Custom-" + src.filename().string();
  File output(::openat(directory.fd, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0644));
  if (output.fd < 0) Error("Create video copy (existing files are never overwritten)");
  try {
    std::array<char, 256 * 1024> buffer{};
    off_t copied = 0;
    while (copied < initial.st_size) {
      if (cancelled.load()) throw std::runtime_error("Copy cancelled");
      auto count = ::read(input.fd, buffer.data(),
                          std::min<off_t>(buffer.size(), initial.st_size - copied));
      if (count < 0 && errno == EINTR) continue;
      if (count < 0) Error("Read video");
      if (count == 0) throw std::runtime_error("Video changed during copy");
      for (ssize_t written = 0; written < count;) {
        auto n = ::write(output.fd, buffer.data() + written, count - written);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) Error("Write video copy");
        if (n == 0) throw std::runtime_error("Could not write video copy");
        written += n;
      }
      copied += count;
    }
    struct stat after{};
    if (::fstat(input.fd, &after) != 0) Error("Check source after copy");
    if (initial.st_size != after.st_size || initial.st_mtime != after.st_mtime)
      throw std::runtime_error("Source changed during copy; no partial file retained");
    if (::fsync(output.fd) != 0) Error("Flush video copy");
    if (cancelled.load()) throw std::runtime_error("Copy cancelled");
    return root / name;
  } catch (...) {
    struct stat owned{}, current{};
    if (::fstat(output.fd, &owned) == 0 &&
        ::fstatat(directory.fd, name.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
        owned.st_ino == current.st_ino && owned.st_dev == current.st_dev)
      ::unlinkat(directory.fd, name.c_str(), 0); // Only our O_EXCL-created partial file.
    throw;
  }
}
}  // namespace Nexora
