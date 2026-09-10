#include "ExternalMedia.hpp"
#include "ProjectionOrientation.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace Nexora;
template<class F> void Reject(F f) { bool threw = false; try { f(); } catch (...) { threw = true; } assert(threw); }
std::string Read(fs::path const& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
int main() {
  auto pattern = (fs::temp_directory_path() / "nexora-external.XXXXXX").string();
  auto* made = ::mkdtemp(pattern.data());
  assert(made);
  auto root = fs::canonical(made);
  struct Cleanup { fs::path p; ~Cleanup() { fs::remove_all(p); } } cleanup{root};
  fs::create_directory(root / "Movies"); fs::create_directory(root / "Song");
  auto video = root / "Movies" / "A video (360).MP4";
  std::string bytes(1000000, 'v');
  std::ofstream(video, std::ios::binary) << bytes;
  std::ofstream(root / "Song" / "Info.dat") << "{\"original\":true}";
  auto info = Read(root / "Song" / "Info.dat");
  ExternalSelection selection;
  assert(!selection.ForLevel("song-a"));
  selection.Bind({"song-a", video, 0, 0});
  assert(selection.ForLevel("song-a") && selection.ForLevel("song-a")->path == video);
  assert(!selection.ForLevel("song-b") && !selection.ForLevel("song-a-suffix") && !selection.ForLevel(""));
  // Binding/selecting/browsing has no copying/moving/importing side effects.
  assert(std::distance(fs::directory_iterator(root / "Song"), fs::directory_iterator{}) == 1);
  assert(Read(video) == bytes && Read(root / "Song" / "Info.dat") == info);
  auto list = BrowseMedia(root / "Movies");
  assert(list.entries.size() == 1 && list.entries[0].path == video && !list.entries[0].directory);
  assert(BrowseMedia(root / "Movies", 0).truncated);
  Reject([&] { selection.Bind({"song-b", root / "missing.mp4", 0, 0}); });
  assert(selection.ForLevel("song-a")); // failed replacement preserves the old selection
  Reject([&] { selection.Bind({"", video, 0, 0}); });
  Reject([&] { selection.Bind({"x", video, std::numeric_limits<float>::quiet_NaN(), 0}); });
  Reject([&] { ReadableExternalVideo("relative.mp4"); });
  Reject([&] { ReadableExternalVideo(root / "Song" / "Info.dat"); });
  std::ofstream(root / "empty.mp4");
  Reject([&] { ReadableExternalVideo(root / "empty.mp4"); });
  Reject([&] { ReadableExternalVideo(root / "bad\n.mp4"); });
  std::atomic_bool cancel = true;
  Reject([&] { CopyVideoToMap(video, root / "Song", cancel); });
  assert(!fs::exists(root / "Song" / "Nexora-Custom-A video (360).MP4"));
  cancel = false;
  auto copy = CopyVideoToMap(video, root / "Song", cancel);
  assert(copy.parent_path() == root / "Song" && Read(copy) == bytes && Read(video) == bytes);
  assert(Read(root / "Song" / "Info.dat") == info);
  std::ofstream(copy, std::ios::binary | std::ios::trunc) << "existing user copy";
  Reject([&] { CopyVideoToMap(video, root / "Song", cancel); });
  assert(Read(copy) == "existing user copy");
  Reject([&] { CopyVideoToMap(video, root / "Movies", cancel); });
  auto second = root / "Movies" / "link.mp4";
  std::ofstream(second) << "source";
  fs::create_symlink(video, root / "Song" / "Nexora-Custom-link.mp4");
  Reject([&] { CopyVideoToMap(second, root / "Song", cancel); });
  assert(Read(video) == bytes);
  selection.Clear(); assert(!selection.Current());

  assert(ProjectionCorrection("", 0) == 180);
  assert(ProjectionCorrection("", 180) == 0); // existing workaround not doubled
  assert(ProjectionCorrection("", -180) == 0);
  assert(ProjectionCorrection("", 540) == 0);
  assert(ProjectionCorrection("u0-forward", 0) == 0); // new capture direction preserved
  assert(ProjectionCorrection("center-forward", 180) == 180); // explicit metadata authoritative
  Reject([] { ProjectionCorrection("made-up", 0); });
  // In the existing dome: centre U=.5 has Z=-1. A half turn puts it at +Z,
  // while rotation about Y leaves vertical orientation unchanged.
  double pi = std::acos(-1.0);
  assert(std::abs(-std::cos(ProjectionCorrection("", 0) * pi / 180) - 1) < 1e-6);
  std::cout << "external in-place selection, exclusive copy, exact song identity and orientation tests passed\n";
}
