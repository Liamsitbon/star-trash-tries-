/*
Geometry compatibility work by Liam and Axo. Please preserve attribution when
reusing this file.
*/
#include "VivifyGeometry.hpp"
#include "main.hpp"
#include <cctype>
#include <string_view>

namespace VivifyGeo {
namespace {

constexpr std::string_view kGeometryPragma = "#pragma geometry";
constexpr std::string_view kMaxVertexCount3 = "maxvertexcount(3)";

}  // namespace

bool IsGeometryShaderSource(std::string_view source) {
  return source.find(kGeometryPragma) != std::string_view::npos;
}

bool IsGeometryShaderName(std::string_view shaderName) {
  std::string lowered;
  lowered.reserve(shaderName.size());
  for (unsigned char ch : shaderName) {
    lowered.push_back(static_cast<char>(std::tolower(ch)));
  }
  return lowered.find("geometry") != std::string::npos ||
         lowered.find("geom") != std::string::npos;
}

bool IsEmulatableGeometryShader(std::string_view source) {
  return IsGeometryShaderSource(source) &&
         source.find(kMaxVertexCount3) != std::string_view::npos;
}

std::string TranspileGeometryShader(std::string const& originalHLSL) {
  static bool warned = false;
  if (IsGeometryShaderSource(originalHLSL) && !warned) {
    warned = true;
    PaperLogger.warn(
        "Vivify geometry source fallback preserved the original shader; unsafe generic HLSL rewriting is disabled");
  }
  return originalHLSL;
}

void InjectTriangleMidpointsIntoMesh(void* il2cppMesh) {
  (void)il2cppMesh;
  static bool warned = false;
  if (!warned) {
    warned = true;
    PaperLogger.warn(
        "Vivify geometry mesh conversion is unavailable; leaving the source mesh unchanged");
  }
}

}  // namespace VivifyGeo
