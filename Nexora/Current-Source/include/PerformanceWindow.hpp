#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
namespace Nexora {
struct PerformanceWindow {
  static constexpr std::size_t Size=240;
  std::array<double,Size> updateMs{}, frameIntervalMs{};
  std::size_t count=0;
  bool Add(double update, double interval) {
    if (!std::isfinite(update) || !std::isfinite(interval) || update<0 || interval<=0) return false;
    updateMs[count]=update; frameIntervalMs[count]=interval;
    ++count;
    if (count < Size) return false;
    count=0; return true;
  }
  static double P95(std::array<double,Size> values) {
    std::sort(values.begin(),values.end()); return values[227];
  }
};
}
