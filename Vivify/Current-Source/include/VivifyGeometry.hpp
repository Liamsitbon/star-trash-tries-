#pragma once

#include <string>
#include <string_view>

namespace VivifyGeo {

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  Vec3() = default;
  Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

  Vec3 operator+(Vec3 const& other) const { return {x + other.x, y + other.y, z + other.z}; }
  Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
};

bool IsGeometryShaderSource(std::string_view source);
bool IsGeometryShaderName(std::string_view shaderName);
bool IsEmulatableGeometryShader(std::string_view source);

// Arbitrary geometry stages cannot be reconstructed safely by source-string
// rewriting. This function therefore fails closed and preserves the source.
std::string TranspileGeometryShader(std::string const& originalHLSL);

// Reserved integration point. Deliberately a no-op until a real mesh
// conversion implementation is available.
void InjectTriangleMidpointsIntoMesh(void* il2cppMesh);

}  // namespace VivifyGeo
