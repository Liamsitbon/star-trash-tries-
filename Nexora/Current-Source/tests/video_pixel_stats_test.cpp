#include "VideoPixelStats.hpp"

#include <cassert>

int main() {
  Nexora::VideoPixelStats stats;
  assert(stats.count == 0 && stats.Mean(0) == 0.0);
  stats.Add(0, 128, 255);
  stats.Add(255, 128, 0);
  assert(stats.count == 2);
  assert(stats.minimum[0] == 0 && stats.maximum[0] == 255);
  assert(stats.minimum[1] == 128 && stats.maximum[1] == 128);
  assert(stats.Mean(0) == 127.5 && stats.Mean(1) == 128.0 && stats.Mean(2) == 127.5);
  Nexora::VideoPixelStats white;
  for (int i = 0; i < 320; ++i) white.Add(255, 255, 255);
  assert(white.count == 320 && white.Mean(0) == 255.0);
  assert(white.minimum == white.maximum);
}
