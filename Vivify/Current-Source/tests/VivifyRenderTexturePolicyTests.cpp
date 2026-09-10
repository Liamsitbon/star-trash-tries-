#include "VivifyRenderTexturePolicy.hpp"
#include <cassert>
#include <limits>
#include <set>
#include <iostream>
using namespace Vivify::RenderTexturePolicy;
// Names only: the runtime uses Unity's actual generated enum, not these values.
enum class F { ARGB32, Depth, ARGBHalf, Shadowmap, RGB565, ARGB4444, ARGB1555, Default,
  ARGB2101010, DefaultHDR, ARGB64, ARGBFloat, RGFloat, RGHalf, RFloat, RHalf, R8,
  ARGBInt, RGInt, RInt, BGRA32, RGB111110Float, RG32, RGBAUShort, RG16, BGRA10101010_XR, BGR101010_XR, R16 };
int main() {
  for (int i=0; i<=static_cast<int>(F::R16); ++i) {
    auto f=static_cast<F>(i);
    assert(Resolve(f,[](auto){return true;})==f);
    assert(!Resolve(f,[](auto){return false;}));
  }
  auto resolve=[](F requested, std::set<F> available) {return Resolve(requested,[&](F f){return available.contains(f);});};
  assert(resolve(F::RFloat,{F::ARGB32,F::RGFloat})==F::RGFloat);
  assert(resolve(F::RHalf,{F::RFloat,F::ARGBHalf})==F::ARGBHalf);
  assert(resolve(F::RGHalf,{F::RGFloat})==F::RGFloat);
  assert(resolve(F::DefaultHDR,{F::ARGBHalf})==F::ARGBHalf);
  assert(resolve(F::RInt,{F::ARGBInt,F::RGFloat})==F::ARGBInt);
  assert(resolve(F::R16,{F::ARGB64})==F::ARGB64);
  assert(resolve(F::BGRA32,{F::ARGB32})==F::ARGB32);
  for(auto f:{F::Depth,F::Shadowmap,F::ARGBFloat,F::RGFloat,F::RFloat,F::ARGBHalf,F::RGHalf,F::RHalf,
              F::DefaultHDR,F::ARGBInt,F::RGInt,F::RInt,F::RGBAUShort,F::ARGB64,F::R16,F::BGRA10101010_XR})
    assert(!resolve(f,{F::ARGB32}));
  assert(ScaledDimension(2048,2,8192)==1024);
  assert(ScaledDimension(2048,.5f,8192)==4096);
  assert(ScaledDimension(2048,std::numeric_limits<float>::denorm_min(),8192)==8192);
  assert(ScaledDimension(2048,std::numeric_limits<float>::quiet_NaN(),8192)==2048);
  assert(ScaledDimension(2048,std::numeric_limits<float>::infinity(),8192)==2048);
  assert(ScaledDimension(2048,-1,8192)==2048);
  assert(ScaledDimension(2048,0,8192)==2048);
  assert(ScaledDimension(2048,std::numeric_limits<float>::max(),8192)==1);
  assert(ScaledDimension(-1,2,8192)==1);
  assert(ScaledDimension(INT32_MAX,2,8192)==4096);
  assert(ScaledDimension(10,1,0)==1);
  std::cout<<"PASS RT format domain and finite-dimension policy\n";
}
