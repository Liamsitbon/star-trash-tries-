#include "CapturePose.hpp"
#include "RgbdVideo.hpp"
#include <cassert>
#include <limits>
int main() {
  using namespace Nexora;
  RgbdVideo depth{true, 2, 20, .8f};
  // Same X head translation, different angular displacement per depth region.
  auto nearAngle = std::atan2(-.1f, depth.Distance(0));
  auto farAngle = std::atan2(-.1f, depth.Distance(1));
  assert(std::abs(nearAngle) > std::abs(farAngle)*9);
  for (int q=0; q<=2; ++q) { depth.quality=q;
    assert((depth.Rings()+1)*(depth.Segments()+1) < 65536); }
  depth.worldScale=4; assert(depth.IsValid());
  depth.strength=std::numeric_limits<float>::infinity(); assert(!depth.IsValid());
  std::vector<CapturePose> poses{{0, {0,0,0}}, {10, {10,0,0}, {0,0,0,-1}}};
  assert(ValidCapturePoses(poses));
  assert(SampleCapturePose(poses,5).position[0]==5);
  assert(SampleCapturePose(poses,5).rotation[3]==1);
  assert(SampleCapturePose(poses,1).position[0]==1); // backwards seek, no accumulator
  assert(SampleCapturePose(poses,1).position == SampleCapturePose(poses,1).position); // pause
  poses[1].time=0; assert(!ValidCapturePoses(poses));
}
