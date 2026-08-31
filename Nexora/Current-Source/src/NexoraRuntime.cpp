#include "NexoraRuntime.hpp"

#include "NexoraComponents.hpp"
#include "QuestInterop.hpp"
#include "main.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/Video/VideoAudioOutputMode.hpp"
#include "UnityEngine/Video/VideoRenderMode.hpp"
#include "UnityEngine/Video/VideoSource.hpp"
#include "GlobalNamespace/BeatmapCallbacksUpdater.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "songcore/shared/Capabilities.hpp"
#include "songcore/shared/SongCore.hpp"
#include "custom-types/shared/delegate.hpp"

namespace Nexora {
namespace {

constexpr float kSeekInFlightTimeoutSeconds = 2.0f;
constexpr std::size_t kMaximumEventStringBytes = 512;
constexpr std::size_t kMaximumDomeIdBytes = 64;

constexpr std::string_view kAssetsPath =
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/Nexora/Assets/nexoraassets.android";
constexpr std::string_view kAssetsAlternatePath =
    "/storage/emulated/0/ModData/com.beatgames.beatsaber/Mods/Nexora/Assets/nexoraassets.android";
constexpr std::string_view kDomeMaterialAsset = "assets/nexora/materials/nexoradome.mat";

// Cached Shader Property IDs
int s_propOpacity = 0;
int s_propBrightness = 0;
int s_propExposure = 0;
int s_propSaturation = 0;
int s_propHueShift = 0;
int s_propProjectionMode = 0;
int s_propDeformAmplitude = 0;
int s_propDeformFrequency = 0;
int s_propDeformSpeed = 0;
int s_propRippleAmount = 0;
int s_propRippleFrequency = 0;
int s_propRippleSpeed = 0;
int s_propTwist = 0;
int s_propPinch = 0;
int s_propPulse = 0;
int s_propKaleidoscope = 0;
int s_propPixelate = 0;
int s_propChromatic = 0;
int s_propScanline = 0;
int s_propVignette = 0;
int s_propFog = 0;
int s_propTint = 0;
int s_propSrcBlend = 0;
int s_propDstBlend = 0;
int s_propMainTex = 0;
int s_propFlipX = 0;
int s_propFlipY = 0;
int s_propSwapEyes = 0;
int s_propVideoReady = 0;

int s_propCameraAmount = 0;
int s_propCameraFisheye = 0;
int s_propCameraChromatic = 0;
int s_propCameraGlitch = 0;
int s_propCameraVignette = 0;
int s_propCameraScanline = 0;
int s_propCameraPixelate = 0;
int s_propCameraGrayscale = 0;
int s_propCameraExposure = 0;
int s_propCameraHueShift = 0;
int s_propCameraSplit = 0;
int s_propCameraShake = 0;
int s_propCameraSwirl = 0;
int s_propCameraKaleidoscope = 0;
int s_propCameraTint = 0;

template <typename T>
bool Alive(T* object) {
  return object != nullptr &&
         UnityEngine::Object::op_Implicit_bool(static_cast<UnityEngine::Object*>(object));
}

template <typename T>
bool Alive(SafePtrUnity<T> const& object) {
  return object.isAlive();
}

float Clamp(float value, float minimum, float maximum) {
  return std::clamp(value, minimum, maximum);
}

std::optional<float> ReadFloat(rapidjson::Value const& object, char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd()) return std::nullopt;
  if (iterator->value.IsNumber()) {
    float const value = iterator->value.GetFloat();
    return std::isfinite(value) ? std::optional<float>(value) : std::nullopt;
  }
  if (iterator->value.IsBool()) return iterator->value.GetBool() ? 1.0f : 0.0f;
  return std::nullopt;
}

std::optional<bool> ReadBool(rapidjson::Value const& object, char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd()) return std::nullopt;
  if (iterator->value.IsBool()) return iterator->value.GetBool();
  if (iterator->value.IsNumber()) {
    double const value = iterator->value.GetDouble();
    return std::isfinite(value) ? std::optional<bool>(value != 0.0) : std::nullopt;
  }
  return std::nullopt;
}

std::optional<std::string> ReadString(
    rapidjson::Value const& object, char const* key,
    std::size_t maximumBytes = kMaximumEventStringBytes) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsString() ||
      iterator->value.GetStringLength() > maximumBytes) {
    return std::nullopt;
  }
  return std::string(iterator->value.GetString(), iterator->value.GetStringLength());
}

std::optional<UnityEngine::Vector3> ReadVector3(rapidjson::Value const& object,
                                                char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsArray() ||
      iterator->value.Size() < 3) {
    return std::nullopt;
  }
  auto array = iterator->value.GetArray();
  if (!array[0].IsNumber() || !array[1].IsNumber() || !array[2].IsNumber()) {
    return std::nullopt;
  }
  float const x = array[0].GetFloat();
  float const y = array[1].GetFloat();
  float const z = array[2].GetFloat();
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
    return std::nullopt;
  }
  return UnityEngine::Vector3(Clamp(x, -10000.0f, 10000.0f),
                              Clamp(y, -10000.0f, 10000.0f),
                              Clamp(z, -10000.0f, 10000.0f));
}

std::optional<UnityEngine::Color> ReadColor(rapidjson::Value const& object,
                                            char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsArray() ||
      iterator->value.Size() < 3 || iterator->value.Size() > 4) {
    return std::nullopt;
  }
  auto array = iterator->value.GetArray();
  float components[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  for (rapidjson::SizeType index = 0; index < array.Size(); ++index) {
    if (!array[index].IsNumber()) return std::nullopt;
    components[index] = array[index].GetFloat();
    if (!std::isfinite(components[index])) return std::nullopt;
  }
  return UnityEngine::Color(Clamp(components[0], 0.0f, 8.0f),
                            Clamp(components[1], 0.0f, 8.0f),
                            Clamp(components[2], 0.0f, 8.0f),
                            Clamp(components[3], 0.0f, 1.0f));
}

Ease ReadEase(rapidjson::Value const& object) {
  auto value = ReadString(object, "ease").value_or("linear");
  if (value == "easeInOut" || value == "inOut" || value == "smooth") return Ease::InOut;
  if (value == "easeOut" || value == "outCubic") return Ease::OutCubic;
  if (value == "easeIn" || value == "inCubic") return Ease::InCubic;
  if (value == "smoothStep") return Ease::SmoothStep;
  return Ease::Linear;
}

float EaseProgress(Ease ease, float value) {
  value = Clamp(value, 0.0f, 1.0f);
  switch (ease) {
    case Ease::InOut:
      return value < 0.5f ? 4.0f * value * value * value
                          : 1.0f - std::pow(-2.0f * value + 2.0f, 3.0f) / 2.0f;
    case Ease::OutCubic:
      return 1.0f - std::pow(1.0f - value, 3.0f);
    case Ease::InCubic:
      return value * value * value;
    case Ease::SmoothStep:
      return value * value * (3.0f - 2.0f * value);
    default:
      return value;
  }
}

float Lerp(float from, float to, float progress) { return from + (to - from) * progress; }

UnityEngine::Color LerpColor(UnityEngine::Color const& from,
                             UnityEngine::Color const& to, float progress) {
  return UnityEngine::Color(Lerp(from.r, to.r, progress), Lerp(from.g, to.g, progress),
                            Lerp(from.b, to.b, progress), Lerp(from.a, to.a, progress));
}

DomeVisual LerpVisual(DomeVisual const& from, DomeVisual const& to, float progress) {
  DomeVisual result;
#define NEXORA_LERP_DOME(field) result.field = Lerp(from.field, to.field, progress)
  NEXORA_LERP_DOME(radius);
  NEXORA_LERP_DOME(opacity);
  NEXORA_LERP_DOME(brightness);
  NEXORA_LERP_DOME(exposure);
  NEXORA_LERP_DOME(saturation);
  NEXORA_LERP_DOME(hueShift);
  NEXORA_LERP_DOME(yaw);
  NEXORA_LERP_DOME(pitch);
  NEXORA_LERP_DOME(roll);
  NEXORA_LERP_DOME(scaleX);
  NEXORA_LERP_DOME(scaleY);
  NEXORA_LERP_DOME(scaleZ);
  NEXORA_LERP_DOME(deform);
  NEXORA_LERP_DOME(deformFrequency);
  NEXORA_LERP_DOME(deformSpeed);
  NEXORA_LERP_DOME(ripple);
  NEXORA_LERP_DOME(rippleFrequency);
  NEXORA_LERP_DOME(rippleSpeed);
  NEXORA_LERP_DOME(twist);
  NEXORA_LERP_DOME(pinch);
  NEXORA_LERP_DOME(pulse);
  NEXORA_LERP_DOME(kaleidoscope);
  NEXORA_LERP_DOME(pixelate);
  NEXORA_LERP_DOME(chromatic);
  NEXORA_LERP_DOME(scanline);
  NEXORA_LERP_DOME(vignette);
  NEXORA_LERP_DOME(fog);
  NEXORA_LERP_DOME(projection);
  NEXORA_LERP_DOME(flipX);
  NEXORA_LERP_DOME(flipY);
  NEXORA_LERP_DOME(swapEyes);
#undef NEXORA_LERP_DOME
  result.tint = LerpColor(from.tint, to.tint, progress);
  return result;
}

CameraVisual LerpVisual(CameraVisual const& from, CameraVisual const& to,
                        float progress) {
  CameraVisual result;
#define NEXORA_LERP_CAMERA(field) result.field = Lerp(from.field, to.field, progress)
  NEXORA_LERP_CAMERA(amount);
  NEXORA_LERP_CAMERA(fisheye);
  NEXORA_LERP_CAMERA(chromatic);
  NEXORA_LERP_CAMERA(glitch);
  NEXORA_LERP_CAMERA(vignette);
  NEXORA_LERP_CAMERA(scanline);
  NEXORA_LERP_CAMERA(pixelate);
  NEXORA_LERP_CAMERA(grayscale);
  NEXORA_LERP_CAMERA(exposure);
  NEXORA_LERP_CAMERA(hueShift);
  NEXORA_LERP_CAMERA(split);
  NEXORA_LERP_CAMERA(shake);
  NEXORA_LERP_CAMERA(swirl);
  NEXORA_LERP_CAMERA(kaleidoscope);
#undef NEXORA_LERP_CAMERA
  result.tint = LerpColor(from.tint, to.tint, progress);
  return result;
}

bool SafeMapMediaPath(std::string const& name) {
  if (name.empty() || name.size() > 512 ||
      name.find('\\') != std::string::npos ||
      name.find(':') != std::string::npos || name.front() == '~' ||
      std::any_of(name.begin(), name.end(), [](unsigned char character) {
        return character < 0x20 || character == 0x7f;
      })) {
    return false;
  }
  std::filesystem::path const relative(name);
  if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
    return false;
  }
  for (auto const& component : relative) {
    if (component == "." || component == "..") return false;
  }
  static std::unordered_set<std::string> const extensions = {".mp4", ".m4v", ".mov",
                                                             ".webm"};
  std::string extension = relative.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
  return extensions.contains(extension);
}

bool IsPathInside(std::filesystem::path const& root,
                  std::filesystem::path const& candidate) {
  auto rootIterator = root.begin();
  auto candidateIterator = candidate.begin();
  for (; rootIterator != root.end(); ++rootIterator, ++candidateIterator) {
    if (candidateIterator == candidate.end() || *rootIterator != *candidateIterator) {
      return false;
    }
  }
  return true;
}

bool IsReadableRegularFile(std::filesystem::path const& path) {
  std::string const nativePath = path.string();
  if (nativePath.empty() || access(nativePath.c_str(), R_OK) != 0) return false;

  std::error_code error;
  bool const regular = std::filesystem::is_regular_file(path, error);
  if (!error) return regular;

  struct stat info {};
  return stat(nativePath.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool IsSafeDomeId(std::string const& id) {
  return !id.empty() && id.size() <= kMaximumDomeIdBytes &&
         std::none_of(id.begin(), id.end(), [](unsigned char character) {
           return character < 0x20 || character == 0x7f;
         });
}

double NormalizeVideoTime(UnityEngine::Video::VideoPlayer* player,
                          double desired, bool looping) {
  if (!std::isfinite(desired)) {
    throw std::runtime_error("video time is not finite");
  }
  desired = std::max(0.0, desired);
  if (!Alive(player) || !player->get_isPrepared()) return desired;

  double const length = player->get_length();
  if (!std::isfinite(length) || length <= 0.001) return desired;
  if (looping) return std::fmod(desired, length);
  return std::min(desired, std::max(0.0, std::nextafter(length, 0.0)));
}

bool NeedsVideoSeek(UnityEngine::Video::VideoPlayer* player, double desired) {
  if (!Alive(player) || !player->get_isPrepared() || !player->get_canSetTime()) {
    return false;
  }
  double const current = player->get_time();
  return !std::isfinite(current) || std::fabs(current - desired) > 0.05;
}

std::string SafeManagedString(StringW value) noexcept {
  if (!value) return "<no error message>";
  try {
    return std::string(value);
  } catch (...) {
    return "<unreadable error message>";
  }
}

std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

}  // namespace

Runtime& Runtime::Instance() {
  static Runtime runtime;
  return runtime;
}

void Runtime::InitPropertyIds() {
  if (_propertyIdsInitialized) return;

  s_propOpacity = UnityEngine::Shader::PropertyToID(StringW("_Opacity"));
  s_propBrightness = UnityEngine::Shader::PropertyToID(StringW("_Brightness"));
  s_propExposure = UnityEngine::Shader::PropertyToID(StringW("_Exposure"));
  s_propSaturation = UnityEngine::Shader::PropertyToID(StringW("_Saturation"));
  s_propHueShift = UnityEngine::Shader::PropertyToID(StringW("_HueShift"));
  s_propProjectionMode = UnityEngine::Shader::PropertyToID(StringW("_ProjectionMode"));
  s_propDeformAmplitude = UnityEngine::Shader::PropertyToID(StringW("_DeformAmplitude"));
  s_propDeformFrequency = UnityEngine::Shader::PropertyToID(StringW("_DeformFrequency"));
  s_propDeformSpeed = UnityEngine::Shader::PropertyToID(StringW("_DeformSpeed"));
  s_propRippleAmount = UnityEngine::Shader::PropertyToID(StringW("_RippleAmount"));
  s_propRippleFrequency = UnityEngine::Shader::PropertyToID(StringW("_RippleFrequency"));
  s_propRippleSpeed = UnityEngine::Shader::PropertyToID(StringW("_RippleSpeed"));
  s_propTwist = UnityEngine::Shader::PropertyToID(StringW("_Twist"));
  s_propPinch = UnityEngine::Shader::PropertyToID(StringW("_Pinch"));
  s_propPulse = UnityEngine::Shader::PropertyToID(StringW("_Pulse"));
  s_propKaleidoscope = UnityEngine::Shader::PropertyToID(StringW("_Kaleidoscope"));
  s_propPixelate = UnityEngine::Shader::PropertyToID(StringW("_Pixelate"));
  s_propChromatic = UnityEngine::Shader::PropertyToID(StringW("_Chromatic"));
  s_propScanline = UnityEngine::Shader::PropertyToID(StringW("_Scanline"));
  s_propVignette = UnityEngine::Shader::PropertyToID(StringW("_Vignette"));
  s_propFog = UnityEngine::Shader::PropertyToID(StringW("_Fog"));
  s_propTint = UnityEngine::Shader::PropertyToID(StringW("_Tint"));
  s_propSrcBlend = UnityEngine::Shader::PropertyToID(StringW("_SrcBlend"));
  s_propDstBlend = UnityEngine::Shader::PropertyToID(StringW("_DstBlend"));
  s_propMainTex = UnityEngine::Shader::PropertyToID(StringW("_MainTex"));
  s_propFlipX = UnityEngine::Shader::PropertyToID(StringW("_FlipX"));
  s_propFlipY = UnityEngine::Shader::PropertyToID(StringW("_FlipY"));
  s_propSwapEyes = UnityEngine::Shader::PropertyToID(StringW("_SwapEyes"));
  s_propVideoReady = UnityEngine::Shader::PropertyToID(StringW("_VideoReady"));

  s_propCameraAmount = UnityEngine::Shader::PropertyToID(StringW("_CameraAmount"));
  s_propCameraFisheye = UnityEngine::Shader::PropertyToID(StringW("_CameraFisheye"));
  s_propCameraChromatic = UnityEngine::Shader::PropertyToID(StringW("_CameraChromatic"));
  s_propCameraGlitch = UnityEngine::Shader::PropertyToID(StringW("_CameraGlitch"));
  s_propCameraVignette = UnityEngine::Shader::PropertyToID(StringW("_CameraVignette"));
  s_propCameraScanline = UnityEngine::Shader::PropertyToID(StringW("_CameraScanline"));
  s_propCameraPixelate = UnityEngine::Shader::PropertyToID(StringW("_CameraPixelate"));
  s_propCameraGrayscale = UnityEngine::Shader::PropertyToID(StringW("_CameraGrayscale"));
  s_propCameraExposure = UnityEngine::Shader::PropertyToID(StringW("_CameraExposure"));
  s_propCameraHueShift = UnityEngine::Shader::PropertyToID(StringW("_CameraHueShift"));
  s_propCameraSplit = UnityEngine::Shader::PropertyToID(StringW("_CameraSplit"));
  s_propCameraShake = UnityEngine::Shader::PropertyToID(StringW("_CameraShake"));
  s_propCameraSwirl = UnityEngine::Shader::PropertyToID(StringW("_CameraSwirl"));
  s_propCameraKaleidoscope =
      UnityEngine::Shader::PropertyToID(StringW("_CameraKaleidoscope"));
  s_propCameraTint = UnityEngine::Shader::PropertyToID(StringW("_CameraTint"));
  // Set the retry guard only after every Unity call succeeded. A domain-load
  // exception partway through initialization must not leave zero property IDs
  // permanently cached for the rest of the Beat Saber process.
  _propertyIdsInitialized = true;
}

UnityEngine::Mesh* Runtime::CreateProceduralDomeMesh(int rings, int segments, float radius) {
  rings = std::clamp(rings, 16, 128);
  segments = std::clamp(segments, 16, 128);

  int const vertexCount = (rings + 1) * (segments + 1);
  int const triangleIndexCount = rings * segments * 6;

  auto vertices = ArrayW<UnityEngine::Vector3>(vertexCount);
  auto uvs = ArrayW<UnityEngine::Vector2>(vertexCount);
  auto normals = ArrayW<UnityEngine::Vector3>(vertexCount);
  auto triangles = ArrayW<int32_t>(triangleIndexCount);

  float const dPhi = static_cast<float>(M_PI) / static_cast<float>(rings);
  float const dTheta = static_cast<float>(2.0 * M_PI) / static_cast<float>(segments);

  int vIndex = 0;
  for (int r = 0; r <= rings; ++r) {
    float const phi = static_cast<float>(r) * dPhi;
    float const sinPhi = std::sin(phi);
    float const cosPhi = std::cos(phi);
    float const v = 1.0f - static_cast<float>(r) / static_cast<float>(rings);

    for (int s = 0; s <= segments; ++s) {
      float const theta = static_cast<float>(s) * dTheta;
      float const sinTheta = std::sin(theta);
      float const cosTheta = std::cos(theta);
      float const u = static_cast<float>(s) / static_cast<float>(segments);

      float const x = sinPhi * sinTheta;
      float const y = cosPhi;
      float const z = sinPhi * cosTheta;

      vertices[vIndex] = UnityEngine::Vector3(x * radius, y * radius, z * radius);
      uvs[vIndex] = UnityEngine::Vector2(u, v);
      normals[vIndex] = UnityEngine::Vector3(-x, -y, -z);
      ++vIndex;
    }
  }

  int tIndex = 0;
  for (int r = 0; r < rings; ++r) {
    for (int s = 0; s < segments; ++s) {
      int const current = r * (segments + 1) + s;
      int const next = current + segments + 1;

      // Inward-facing winding for viewing from the inside center
      triangles[tIndex++] = current;
      triangles[tIndex++] = current + 1;
      triangles[tIndex++] = next;

      triangles[tIndex++] = current + 1;
      triangles[tIndex++] = next + 1;
      triangles[tIndex++] = next;
    }
  }

  auto* mesh = UnityEngine::Mesh::New_ctor();
  if (!Alive(mesh)) throw std::runtime_error("Unity could not allocate the dome mesh");
  try {
    mesh->set_name(StringW("NexoraInwardDomeMesh"));
    mesh->set_vertices(vertices);
    mesh->set_uv(uvs);
    mesh->set_normals(normals);
    mesh->set_triangles(triangles);
    mesh->RecalculateBounds();
  } catch (...) {
    UnityEngine::Object::Destroy(mesh);
    throw;
  }
  return mesh;
}

void Runtime::LateLoad() {
  if (GetNexoraEnabled()) {
    EnsureBehaviour();
    LoadAssets();
  } else if (SongCore::API::Capabilities::IsCapabilityRegistered(kCapability)) {
    SongCore::API::Capabilities::UnregisterCapability(kCapability);
  }
  CustomJSONData::CustomEventCallbacks::AddCustomEventCallback(&Runtime::OnCustomEventStatic);
  SongCore::API::LevelSelect::GetLevelWasSelectedEvent() +=
      [](SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
        auto& runtime = Runtime::Instance();
        bool requiresNexora = false;
        try {
          std::string mapRoot;
          if (event.isCustom && event.customBeatmapLevel != nullptr) {
            mapRoot = std::string(event.customBeatmapLevel->customLevelPath);
          }
          if (event.isCustom && event.customLevelDetails) {
            auto const& requirements =
                event.customLevelDetails->difficultyDetails.requirements;
            auto const context = QuestModInterop::Inspect(requirements);
            requiresNexora = std::any_of(
                requirements.begin(), requirements.end(), [](auto const& requirement) {
                  return requirement == std::string(kCapability);
                });
            PaperLogger.info(
                "Nexora interop: installed[C={} N={} NE={} V={}] required[C={} N={} NE={} V={}]",
                context.installed.cinema, context.installed.nexora,
                context.installed.noodleExtensions, context.installed.vivify,
                context.required.cinema, context.required.nexora,
                context.required.noodleExtensions, context.required.vivify);
          }
          runtime.SetSelectedMapRoot(std::move(mapRoot), requiresNexora);

          // Capability registration tells SongCore the mod is installed. Only
          // enable the play button after a required map has passed the private
          // Quest-shader readiness check; enabling it first lets a map launch
          // into the exact black/fallback state this guard is meant to prevent.
          if (!requiresNexora) {
            runtime.SetPlayButtonBlocked(false);
            return;
          }
          if (!GetNexoraEnabled()) {
            runtime.SetPlayButtonBlocked(
                true, "Nexora is disabled in its Quest configuration.");
            return;
          }

          // fileCopies can complete after late-load on some installers. Retry
          // only for maps that actually require Nexora; ordinary SongCore menu
          // selection must not touch Nexora's Unity asset state.
          runtime.LoadAssets();
          if (!runtime.HasQuestShaderAssets()) {
            std::string const failure = runtime.QuestShaderAssetFailure();
            PaperLogger.error("Nexora required-map readiness failed: {}", failure);
            runtime.SetPlayButtonBlocked(true, failure);
            return;
          }
          runtime.SetPlayButtonBlocked(false);
        } catch (std::exception const& exception) {
          PaperLogger.error("Nexora level-selection callback failed safely: {}",
                            exception.what());
          if (requiresNexora) {
            runtime.SetPlayButtonBlocked(
                true, "Nexora could not validate this map safely. Check Nexora.log.");
          }
        } catch (...) {
          PaperLogger.error("Nexora level-selection callback failed safely");
          if (requiresNexora) {
            runtime.SetPlayButtonBlocked(
                true, "Nexora could not validate this map safely. Check Nexora.log.");
          }
        }
      };
  PaperLogger.info(
      "Nexora runtime subscriptions ready; media is map-embedded and Vivify bundles are never loaded");
  // Capability registration is the public readiness signal and deliberately
  // the final potentially-throwing operation. Publish it only after every
  // required event subscription exists, so a partial late-load cannot let
  // SongCore launch a Nexora-required map.
  if (GetNexoraEnabled() &&
      !SongCore::API::Capabilities::IsCapabilityRegistered(kCapability)) {
    SongCore::API::Capabilities::RegisterCapability(kCapability);
  }
}

void Runtime::SetPlayButtonBlocked(bool blocked, std::string reason) {
  if (_playButtonDisabled == blocked) return;
  try {
    if (blocked) {
      if (reason.empty()) reason = "Nexora is not ready for this map.";
      SongCore::API::PlayButton::DisablePlayButton(std::string(kCapability),
                                                   std::move(reason));
    } else {
      SongCore::API::PlayButton::EnablePlayButton(std::string(kCapability));
    }
    _playButtonDisabled = blocked;
  } catch (std::exception const& exception) {
    PaperLogger.error("Nexora could not update SongCore's play-button gate: {}",
                      exception.what());
  } catch (...) {
    PaperLogger.error("Nexora could not update SongCore's play-button gate");
  }
}

bool Runtime::HasQuestShaderAssets() const {
  return Alive(_assetBundle) && Alive(_domeShader) &&
         _domeShader->get_name() == u"Nexora/VideoDome" &&
         _domeShader->get_isSupported();
}

std::string Runtime::QuestShaderAssetFailure() const {
  bool filePresent = false;
  for (auto const path : {kAssetsPath, kAssetsAlternatePath}) {
    if (IsReadableRegularFile(std::filesystem::path(path))) {
      filePresent = true;
      break;
    }
  }
  if (!filePresent) {
    return "Nexora's Quest asset file is missing. Reinstall the complete Nexora QMOD.";
  }
  if (!Alive(_assetBundle)) {
    return "Nexora's Quest asset file exists, but Unity could not load its Android bundle.";
  }
  if (!Alive(_domeShader)) {
    return "Nexora's Android bundle loaded, but Nexora/VideoDome is missing.";
  }
  if (_domeShader->get_name() != u"Nexora/VideoDome") {
    return "Nexora's Android bundle contains the wrong dome shader.";
  }
  if (!_domeShader->get_isSupported()) {
    return "Nexora/VideoDome loaded, but this Quest renderer reports it unsupported.";
  }
  return "Nexora's Quest shader assets are not ready.";
}

void Runtime::SetSelectedMapRoot(std::string mapRoot, bool requiresNexora) {
  if (!mapRoot.empty()) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(mapRoot, error);
    if (error || !std::filesystem::is_directory(normalized, error)) {
      PaperLogger.warn("Nexora rejected selected map root '{}': {}", mapRoot,
                       error ? error.message() : "not a directory");
      mapRoot.clear();
    } else {
      mapRoot = normalized.string();
    }
  }
  if (_selectedMapRoot != mapRoot || _selectedMapRequiresNexora != requiresNexora) {
    ResetSession(false);
  }
  _selectedMapRoot = std::move(mapRoot);
  _selectedMapRequiresNexora = requiresNexora;
  _nextGameplayProbeFrame = -1;
  if (!_selectedMapRoot.empty()) {
    PaperLogger.info("Nexora selected map root='{}' required={}", _selectedMapRoot,
                     _selectedMapRequiresNexora);
  }
}

void Runtime::EnsureBehaviour() {
  if (Alive(_behaviour)) return;
  UnityEngine::GameObject* gameObject = nullptr;
  try {
    gameObject = UnityEngine::GameObject::New_ctor(StringW("NexoraRuntime"));
    if (Alive(gameObject)) {
      _behaviour = gameObject->AddComponent<RuntimeBehaviour*>();
      if (!Alive(_behaviour)) {
        UnityEngine::Object::Destroy(gameObject);
        gameObject = nullptr;
        throw std::runtime_error("Unity could not attach NexoraRuntimeBehaviour");
      }
      UnityEngine::Object::DontDestroyOnLoad(gameObject);
    }
  } catch (std::exception const& exception) {
    if (Alive(gameObject) && !Alive(_behaviour)) {
      try {
        UnityEngine::Object::Destroy(gameObject);
      } catch (...) {
      }
    }
    PaperLogger.warn(
        "Nexora: EnsureBehaviour could not instantiate yet (will retry): {}",
        exception.what());
  } catch (...) {
    if (Alive(gameObject) && !Alive(_behaviour)) {
      try {
        UnityEngine::Object::Destroy(gameObject);
      } catch (...) {
      }
    }
    PaperLogger.warn("Nexora: EnsureBehaviour could not instantiate yet (will retry)");
  }
}

void Runtime::LoadAssets() {
  if (HasQuestShaderAssets()) return;
  try {
    if (!Alive(_assetBundle)) {
      std::string const paths[] = {
          std::string(kAssetsPath),
          std::string(kAssetsAlternatePath)
      };
      for (auto const& path : paths) {
        if (path.empty()) continue;
        if (IsReadableRegularFile(path)) {
          auto bundleRef = UnityEngine::AssetBundle::LoadFromFile(StringW(path));
          auto* b = bundleRef.unsafePtr();
          if (Alive(b)) {
            _assetBundle = b;
            PaperLogger.info("Nexora loaded asset bundle from: {}", path);
            break;
          }
        }
      }
    }

    if (Alive(_assetBundle)) {
      auto domeAsset = _assetBundle->LoadAsset(StringW(std::string(kDomeMaterialAsset)));
      _domeTemplate = il2cpp_utils::try_cast<UnityEngine::Material>(domeAsset.unsafePtr()).value_or(nullptr);
      if (Alive(_domeTemplate)) {
        _domeShader = _domeTemplate->get_shader();
      }
      if (!Alive(_domeShader)) {
        auto domeShaderAsset = _assetBundle->LoadAsset(StringW("assets/nexora/shaders/nexoradome.shader"));
        _domeShader = il2cpp_utils::try_cast<UnityEngine::Shader>(domeShaderAsset.unsafePtr()).value_or(nullptr);
      }
    }

    if (Alive(_domeShader) && _domeShader->get_name() == u"Nexora/VideoDome" &&
        _domeShader->get_isSupported()) {
      _loggedMissingAssets = false;
      PaperLogger.info("Nexora dome shader ready: '{}'", _domeShader->get_name());
    } else {
      if (!_loggedMissingAssets) {
        _loggedMissingAssets = true;
        PaperLogger.error("Nexora required-map asset check: {}", QuestShaderAssetFailure());
      }
    }
  } catch (std::exception const& ex) {
    PaperLogger.warn("Nexora LoadAssets exception: {}", ex.what());
  } catch (...) {
    PaperLogger.warn("Nexora LoadAssets non-standard exception");
  }
}

void Runtime::OnCustomEventStatic(
    GlobalNamespace::BeatmapCallbacksController* callbackController,
    CustomJSONData::CustomEventData* customEventData) {
  try {
    Runtime::Instance().HandleCustomEvent(callbackController, customEventData);
  } catch (std::exception const& exception) {
    PaperLogger.error("Nexora custom-event boundary caught an exception: {}",
                      exception.what());
  } catch (...) {
    PaperLogger.error("Nexora custom-event boundary caught a non-standard exception");
  }
}

CustomJSONData::CustomBeatmapData* Runtime::GetCustomBeatmapData(
    GlobalNamespace::BeatmapCallbacksController* callbackController) const {
  if (callbackController == nullptr || callbackController->_beatmapData == nullptr) {
    return nullptr;
  }
  try {
    return il2cpp_utils::try_cast<CustomJSONData::CustomBeatmapData>(
               callbackController->_beatmapData)
        .value_or(nullptr);
  } catch (...) {
    return nullptr;
  }
}

bool Runtime::PrepareBeatmap(
    GlobalNamespace::BeatmapCallbacksController* callbackController,
    float triggerTime, std::string_view source) {
  if (!std::isfinite(triggerTime)) triggerTime = 0.0f;
  auto* customBeatmapData = GetCustomBeatmapData(callbackController);
  if (customBeatmapData == nullptr) return false;
  if (_currentBeatmapData == customBeatmapData && _callbackController == callbackController &&
      (_lifecycle.IsActive() || _lifecycle.IsSuspended())) {
    return true;
  }

  BeginSession(callbackController);
  if (_callbackController != callbackController || !_lifecycle.IsActive()) return false;

  _currentBeatmapData = customBeatmapData;
  _processedNexoraEvents.clear();
  _preparingBeatmap = true;
  try {
    ReplayMissedEvents(std::max(0.0f, triggerTime) + 0.075f);
    _preparingBeatmap = false;
  } catch (...) {
    _preparingBeatmap = false;
    throw;
  }
  PaperLogger.info(
      "Nexora PrepareBeatmap source={} customEvents={} replayed={} triggerTime={:.3f}",
      source, customBeatmapData->customEventDatas.size(),
      _processedNexoraEvents.size(), triggerTime);
  return true;
}

void Runtime::ReplayMissedEvents(float upToTime) {
  if (_currentBeatmapData == nullptr || _callbackController == nullptr) return;
  for (auto* eventData : _currentBeatmapData->customEventDatas) {
    if (eventData == nullptr || !std::isfinite(eventData->time) ||
        eventData->time > upToTime ||
        !IsNexoraEvent(eventData->type)) {
      continue;
    }
    HandleCustomEvent(_callbackController, eventData);
  }
}

void Runtime::TryPrepareSelectedBeatmapFromScene() {
  if (!GetNexoraEnabled() || _selectedMapRoot.empty() ||
      _currentBeatmapData != nullptr || _nextGameplayProbeFrame == -2) {
    return;
  }
  int const frame = UnityEngine::Time::get_frameCount();
  if (frame < _nextGameplayProbeFrame) return;
  _nextGameplayProbeFrame = frame + 10;

  auto* updater =
      UnityEngine::Object::FindObjectOfType<GlobalNamespace::BeatmapCallbacksUpdater*>();
  if (!Alive(updater) || updater->_beatmapCallbacksController == nullptr) return;
  auto* callbackController = updater->_beatmapCallbacksController;
  auto* customBeatmapData = GetCustomBeatmapData(callbackController);
  if (customBeatmapData == nullptr) return;
  bool const containsNexoraEvents = std::any_of(
      customBeatmapData->customEventDatas.begin(),
      customBeatmapData->customEventDatas.end(), [this](auto* eventData) {
        return eventData != nullptr && IsNexoraEvent(eventData->type);
      });
  if (!containsNexoraEvents) {
    // The active difficulty has no Nexora contract. Cache the negative result
    // until SongCore selects another map instead of scanning ordinary maps for
    // the rest of gameplay.
    _nextGameplayProbeFrame = -2;
    return;
  }
  if (!_selectedMapRequiresNexora) {
    PaperLogger.warn(
        "Nexora recovered an active beatmap containing Nexora events even though SongCore did not report its requirement");
  }
  float const triggerTime = std::max(0.0f, callbackController->get_songTime());
  PrepareBeatmap(callbackController, triggerTime, "gameplay-probe");
}

bool Runtime::IsNexoraEvent(std::string_view type) const {
  static std::unordered_set<std::string_view> const events = {
      kCreateDomeEvent,          kLoadVideoEvent,       kPlayVideoEvent,
      kPauseVideoEvent,         kStopVideoEvent,       kSeekVideoEvent,
      kSetPlaybackEvent,        kSetDomeEvent,         kAnimateDomeEvent,
      kTransitionEvent,         kPulseEvent,           kShockwaveEvent,
      kSetCameraEffectEvent,    kAnimateCameraEffectEvent,
      kGlitchBurstEvent,        kClearCameraEffectEvent,
      kDestroyDomeEvent,        kDestroyAllEvent,
  };
  return events.contains(type);
}

rapidjson::Value const* Runtime::EventJson(
    CustomJSONData::CustomEventData* eventData) const {
  if (eventData == nullptr || eventData->customData == nullptr ||
      !eventData->customData->value.has_value()) {
    return nullptr;
  }
  return &eventData->customData->value.value().get();
}

void Runtime::BeginSession(
    GlobalNamespace::BeatmapCallbacksController* callbackController) {
  if (_callbackController == callbackController &&
      (_lifecycle.IsActive() || _lifecycle.IsSuspended())) {
    return;
  }
  ResetSession(false);
  EnsureBehaviour();
  InitPropertyIds();
  _callbackController = callbackController;
  _audioController =
      UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  _sessionGeneration = _lifecycle.BeginPreparation();
  if (!_lifecycle.Activate(_sessionGeneration)) {
    PaperLogger.error("Nexora session activation was rejected by the lifecycle gate");
    return;
  }
  float const initialSongTime =
      callbackController != nullptr ? callbackController->get_songTime() : 0.0f;
  _lastSongTime = std::isfinite(initialSongTime) ? initialSongTime : 0.0f;
  _paused = false;
  LoadAssets();
  PaperLogger.info("Nexora gameplay session {} started", _sessionGeneration);
}

std::string Runtime::DomeId(rapidjson::Value const& json) const {
  auto iterator = json.FindMember("id");
  if (iterator == json.MemberEnd()) return "main";
  if (!iterator->value.IsString() ||
      iterator->value.GetStringLength() > kMaximumDomeIdBytes) {
    throw std::runtime_error("dome id must be a string no longer than 64 bytes");
  }
  std::string id(iterator->value.GetString(), iterator->value.GetStringLength());
  if (!IsSafeDomeId(id)) {
    throw std::runtime_error("dome id cannot be empty or contain control characters");
  }
  return id;
}

void Runtime::HandleCustomEvent(
    GlobalNamespace::BeatmapCallbacksController* callbackController,
    CustomJSONData::CustomEventData* customEventData) {
  if (!GetNexoraEnabled() || callbackController == nullptr || customEventData == nullptr) {
    return;
  }

  bool const isNexoraEvent = IsNexoraEvent(customEventData->type);
  // A later Vivify event may be the first callback Nexora sees after missing
  // this required map's beat-zero setup. Use it as a preparation signal only
  // for a difficulty that actually requires Nexora. This keeps unrelated
  // custom maps completely outside Nexora's gameplay lifecycle.
  if (!isNexoraEvent && !_selectedMapRequiresNexora) return;
  if (!_preparingBeatmap &&
      (_currentBeatmapData == nullptr || _callbackController != callbackController)) {
    PrepareBeatmap(callbackController, callbackController->get_songTime(),
                   "custom-event-callback");
  }
  if (!isNexoraEvent) return;
  if (_processedNexoraEvents.contains(customEventData)) return;

  auto const* json = EventJson(customEventData);
  if (json == nullptr || !json->IsObject()) {
    PaperLogger.warn("Nexora ignored event '{}' at {:.3f}: data is not an object",
                     std::string(customEventData->type), customEventData->time);
    return;
  }

  try {
    BeginSession(callbackController);
    if (_callbackController != callbackController || !_lifecycle.IsActive()) {
      PaperLogger.warn(
          "Nexora skipped event '{}' because its gameplay lifecycle is not active",
          std::string(customEventData->type));
      return;
    }
    if (!_processedNexoraEvents.emplace(customEventData).second) return;
    std::string_view const type = customEventData->type;
    float const eventTime = customEventData->time;
    if (!std::isfinite(eventTime)) {
      PaperLogger.warn("Nexora ignored event '{}': event time is not finite",
                       std::string(type));
      return;
    }

    if (GetDebugLoggingEnabled()) {
      PaperLogger.info("Nexora event: type='{}' time={:.3f}", type, eventTime);
    }

    if (type == kDestroyAllEvent) {
      DestroyAllDomes();
      _cameraVisual = {};
      _cameraAnimation.active = false;
      return;
    }
    if (type == kClearCameraEffectEvent) {
      _cameraVisual = {};
      _cameraAnimation.active = false;
      return;
    }
    if (type == kSetCameraEffectEvent) {
      SetCameraEffect(*json, eventTime, false);
      return;
    }
    if (type == kAnimateCameraEffectEvent) {
      SetCameraEffect(*json, eventTime, true);
      return;
    }
    if (type == kGlitchBurstEvent) {
      CameraVisual target = _cameraVisual;
      CameraVisual burst = target;
      burst.amount = Clamp(ReadFloat(*json, "amount").value_or(1.0f), 0.0f, 1.0f);
      burst.glitch = Clamp(ReadFloat(*json, "glitch").value_or(1.0f), 0.0f, 1.0f);
      burst.chromatic =
          Clamp(ReadFloat(*json, "chromatic").value_or(0.18f), 0.0f, 0.2f);
      burst.split =
          Clamp(ReadFloat(*json, "split").value_or(0.12f), -0.3f, 0.3f);
      _cameraVisual = burst;
      _cameraAnimation = {burst, target, eventTime,
                          Clamp(ReadFloat(*json, "durationSeconds").value_or(0.35f),
                                0.02f, 30.0f),
                          Ease::OutCubic, true};
      EnsureQuestSafeCameraEffects();
      return;
    }
    if (type == kTransitionEvent) {
      float const opacity =
          Clamp(ReadFloat(*json, "opacity").value_or(0.0f), 0.0f, 1.0f);
      float const duration =
          Clamp(ReadFloat(*json, "durationSeconds").value_or(1.0f),
                0.0f, 120.0f);
      for (auto& [_, layer] : _domes) {
        DomeVisual target = layer.visual;
        target.opacity = opacity;
        layer.animation = {layer.visual, target, eventTime, duration,
                           ReadEase(*json), duration > 0.0f};
        if (duration <= 0.0f) {
          layer.visual = target;
          ApplyDomeVisual(layer);
        }
      }
      // Transition is global. It must never allocate an empty safety dome just
      // because the event omitted an id.
      return;
    }

    std::string const id = DomeId(*json);
    if (type == kDestroyDomeEvent) {
      DestroyDome(id);
      return;
    }

    auto* dome = EnsureDome(id);
    if (dome == nullptr) return;
    if (type == kLoadVideoEvent) {
      LoadVideo(*dome, *json, eventTime);
    } else if (type == kPlayVideoEvent) {
      PlayVideo(*dome, *json, eventTime);
    } else if (type == kPauseVideoEvent) {
      PauseVideo(*dome);
    } else if (type == kStopVideoEvent) {
      StopVideo(*dome);
    } else if (type == kSeekVideoEvent) {
      SeekVideo(*dome, *json);
    } else if (type == kSetPlaybackEvent) {
      SetPlayback(*dome, *json);
    } else if (type == kCreateDomeEvent || type == kSetDomeEvent) {
      ApplyDomeJson(dome->visual, *dome, *json);
      dome->animation.active = false;
      ApplyDomeVisual(*dome);
    } else if (type == kAnimateDomeEvent) {
      AnimateDome(*dome, *json, eventTime);
    } else if (type == kPulseEvent || type == kShockwaveEvent) {
      DomeVisual target = dome->visual;
      DomeVisual burst = target;
      if (type == kPulseEvent) {
        burst.pulse =
            Clamp(ReadFloat(*json, "pulse").value_or(0.18f), -0.8f, 2.0f);
        burst.brightness = Clamp(
            ReadFloat(*json, "brightness")
                .value_or(std::max(1.0f, target.brightness * 1.25f)),
            0.0f, 8.0f);
      } else {
        burst.ripple =
            Clamp(ReadFloat(*json, "ripple").value_or(0.15f), 0.0f, 1.0f);
        burst.rippleFrequency = Clamp(
            ReadFloat(*json, "rippleFrequency").value_or(12.0f), 0.1f, 64.0f);
      }
      dome->visual = burst;
      dome->animation = {burst, target, eventTime,
                         Clamp(ReadFloat(*json, "durationSeconds").value_or(0.6f),
                               0.02f, 30.0f),
                         Ease::OutCubic, true};
    }
  } catch (std::exception const& exception) {
    PaperLogger.error("Nexora event '{}' at {} was skipped safely: {}",
                      std::string(customEventData->type), customEventData->time,
                      exception.what());
  } catch (...) {
    PaperLogger.error("Nexora event '{}' at {} threw a non-standard exception",
                      std::string(customEventData->type), customEventData->time);
  }
}

DomeLayer* Runtime::EnsureDome(std::string const& id) {
  auto existing = _domes.find(id);
  if (existing != _domes.end()) return &existing->second;
  if (_domes.size() >= static_cast<std::size_t>(GetMaxLayers())) {
    PaperLogger.error("Nexora refused dome '{}': configured layer limit is {}", id,
                      GetMaxLayers());
    return nullptr;
  }

  InitPropertyIds();
  UnityEngine::GameObject* object = nullptr;
  UnityEngine::MeshFilter* filter = nullptr;
  UnityEngine::MeshRenderer* renderer = nullptr;
  UnityEngine::Mesh* mesh = nullptr;
  UnityEngine::Material* material = nullptr;
  UnityEngine::Video::VideoPlayer* video = nullptr;
  UnityEngine::Video::VideoPlayer_FrameReadyEventHandler* frameReadyDelegate = nullptr;
  UnityEngine::Video::VideoPlayer_EventHandler* prepareCompletedDelegate = nullptr;
  UnityEngine::Video::VideoPlayer_EventHandler* seekCompletedDelegate = nullptr;
  UnityEngine::Video::VideoPlayer_ErrorEventHandler* errorReceivedDelegate = nullptr;
  bool customShader = false;
  auto cleanup = [&]() noexcept {
    try {
      if (Alive(video)) {
        if (frameReadyDelegate != nullptr) video->remove_frameReady(frameReadyDelegate);
        if (prepareCompletedDelegate != nullptr) {
          video->remove_prepareCompleted(prepareCompletedDelegate);
        }
        if (seekCompletedDelegate != nullptr) {
          video->remove_seekCompleted(seekCompletedDelegate);
        }
        if (errorReceivedDelegate != nullptr) {
          video->remove_errorReceived(errorReceivedDelegate);
        }
        video->set_sendFrameReadyEvents(false);
        video->set_targetMaterialRenderer(nullptr);
        video->Stop();
      }
    } catch (...) {
    }
    try {
      if (Alive(material)) UnityEngine::Object::Destroy(material);
      if (Alive(mesh)) UnityEngine::Object::Destroy(mesh);
      if (Alive(object)) UnityEngine::Object::Destroy(object);
    } catch (...) {
    }
  };

  try {
    object = UnityEngine::GameObject::New_ctor(StringW("NexoraDome_" + id));
    if (!Alive(object)) {
      throw std::runtime_error("Unity could not create the 360 dome GameObject");
    }

    filter = object->AddComponent<UnityEngine::MeshFilter*>();
    renderer = object->AddComponent<UnityEngine::MeshRenderer*>();
    if (!Alive(filter) || !Alive(renderer)) {
      throw std::runtime_error("Nexora dome failed to attach MeshFilter/MeshRenderer");
    }

    int const resolution = GetDomeResolution();
    mesh = CreateProceduralDomeMesh(resolution, resolution, 1.0f);
    if (!Alive(mesh)) throw std::runtime_error("Nexora dome mesh is unavailable");
    filter->set_sharedMesh(mesh);

    if (Alive(_domeShader) && _domeShader->get_isSupported()) {
      // Clone the validated bundle material so its serialized render state and
      // Quest shader variant selection are preserved exactly.
      material = Alive(_domeTemplate)
                     ? UnityEngine::Material::New_ctor(_domeTemplate.ptr())
                     : UnityEngine::Material::New_ctor(_domeShader.ptr());
      customShader = Alive(material);
    }
    if (!Alive(material)) {
      throw std::runtime_error(
          "Nexora could not instantiate its validated Quest dome material");
    }

    material->set_renderQueue(1001);
    if (s_propVideoReady != 0) material->SetFloat(s_propVideoReady, 0.0f);
    renderer->set_sharedMaterial(material);
    renderer->set_receiveShadows(false);
    renderer->set_enabled(false);

    video = object->AddComponent<UnityEngine::Video::VideoPlayer*>();
    if (!Alive(video)) {
      throw std::runtime_error("Unity VideoPlayer component is unavailable");
    }

    // Unity owns both the Android decoder surface and MaterialOverride binding,
    // keeping video frames inside Beat Saber's active Vulkan renderer.
    video->set_source(UnityEngine::Video::VideoSource::Url);
    video->set_renderMode(UnityEngine::Video::VideoRenderMode::MaterialOverride);
    video->set_targetMaterialRenderer(renderer);
    video->set_targetMaterialProperty(StringW("_MainTex"));
    video->set_audioOutputMode(UnityEngine::Video::VideoAudioOutputMode::None);
    video->set_playOnAwake(false);
    video->set_waitForFirstFrame(true);

    std::function<void(UnityEngine::Video::VideoPlayer*, std::int64_t)> frameReady =
        [](UnityEngine::Video::VideoPlayer* player, std::int64_t frameIndex) {
          Runtime::Instance().OnVideoFrameReady(player, frameIndex);
        };
    frameReadyDelegate =
        custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_FrameReadyEventHandler*>(
            frameReady);
    if (frameReadyDelegate == nullptr) {
      throw std::runtime_error("Nexora could not create the frameReady delegate");
    }
    video->add_frameReady(frameReadyDelegate);

    std::function<void(UnityEngine::Video::VideoPlayer*)> prepareCompleted =
        [](UnityEngine::Video::VideoPlayer* player) {
          Runtime::Instance().OnVideoPrepared(player);
        };
    prepareCompletedDelegate =
        custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_EventHandler*>(
            prepareCompleted);
    if (prepareCompletedDelegate == nullptr) {
      throw std::runtime_error("Nexora could not create the prepareCompleted delegate");
    }
    video->add_prepareCompleted(prepareCompletedDelegate);

    std::function<void(UnityEngine::Video::VideoPlayer*)> seekCompleted =
        [](UnityEngine::Video::VideoPlayer* player) {
          Runtime::Instance().OnVideoSeekCompleted(player);
        };
    seekCompletedDelegate =
        custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_EventHandler*>(
            seekCompleted);
    if (seekCompletedDelegate == nullptr) {
      throw std::runtime_error("Nexora could not create the seekCompleted delegate");
    }
    video->add_seekCompleted(seekCompletedDelegate);

    std::function<void(UnityEngine::Video::VideoPlayer*, StringW)> errorReceived =
        [](UnityEngine::Video::VideoPlayer* player, StringW message) {
          Runtime::Instance().OnVideoError(player, message);
        };
    errorReceivedDelegate =
        custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_ErrorEventHandler*>(
            errorReceived);
    if (errorReceivedDelegate == nullptr) {
      throw std::runtime_error("Nexora could not create the errorReceived delegate");
    }
    video->add_errorReceived(errorReceivedDelegate);

    DomeLayer layer;
    layer.id = id;
    layer.object = object;
    layer.filter = filter;
    layer.mesh = mesh;
    layer.renderer = renderer;
    layer.material = material;
    layer.video = video;
    layer.frameReadyDelegate = frameReadyDelegate;
    layer.prepareCompletedDelegate = prepareCompletedDelegate;
    layer.seekCompletedDelegate = seekCompletedDelegate;
    layer.errorReceivedDelegate = errorReceivedDelegate;
    layer.customShader = customShader;
    auto [iterator, inserted] = _domes.emplace(id, std::move(layer));
    if (!inserted) {
      cleanup();
      return &iterator->second;
    }
    ApplyDomeVisual(iterator->second);
    PaperLogger.info(
        "Nexora created procedural dome '{}' (res={} videoPipeline=UnityMaterialOverride safetyBackdrop=true layers={}/{})",
        id, resolution, _domes.size(), GetMaxLayers());
    return &iterator->second;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::string Runtime::ResolveMediaUrl(rapidjson::Value const& json) const {
  auto media = ReadString(json, "media");
  if (!media.has_value() || media->empty()) {
    throw std::runtime_error("LoadVideo requires a media path");
  }

  std::string const mediaPath = *media;
  if (!SafeMapMediaPath(mediaPath)) {
    throw std::runtime_error(
        "media must be a safe map-relative MP4/M4V/MOV/WebM path: " + mediaPath);
  }

  if (_selectedMapRoot.empty()) {
    throw std::runtime_error("selected custom map path is unavailable; media cannot be resolved");
  }

  std::error_code ec;
  auto root = std::filesystem::weakly_canonical(_selectedMapRoot, ec);
  if (ec || !std::filesystem::is_directory(root, ec)) {
    throw std::runtime_error("selected custom map root is no longer readable");
  }

  // The authored path is authoritative. For MBF-installed maps, also accept
  // the required root duplicate when the DAT references Nexora/Media/name.mp4.
  std::vector<std::filesystem::path> relativeCandidates = {
      std::filesystem::path(mediaPath)};
  auto const authored = std::filesystem::path(mediaPath).lexically_normal();
  if (authored.parent_path() == std::filesystem::path("Nexora/Media")) {
    relativeCandidates.emplace_back(authored.filename());
  }

  for (std::size_t index = 0; index < relativeCandidates.size(); ++index) {
    ec.clear();
    auto candidate = std::filesystem::weakly_canonical(root / relativeCandidates[index], ec);
    if (ec || !IsPathInside(root, candidate) || !IsReadableRegularFile(candidate)) {
      continue;
    }
    if (index != 0) {
      PaperLogger.warn(
          "Nexora media '{}' was missing; using the MBF root duplicate '{}'",
          mediaPath, candidate.filename().string());
    }
    return candidate.string();
  }

  throw std::runtime_error("map media file not found inside selected map: " + mediaPath);
}

void Runtime::LoadVideo(DomeLayer& dome, rapidjson::Value const& json,
                        float eventTime) {
  if (!Alive(dome.video)) throw std::runtime_error("dome VideoPlayer was destroyed");
  auto queuedAnimation = dome.animation;
  bool const animationArrivedFirst = queuedAnimation.active &&
      std::fabs(queuedAnimation.startSongTime - eventTime) <= 0.001f;
  StopVideo(dome);
  ApplyDomeJson(dome.visual, dome, json);
  if (animationArrivedFirst) {
    // CJD does not promise a stable callback order for equal-beat custom
    // events. Dynasty authors LoadVideo(opacity=0) and AnimateDome(opacity=1)
    // at beat zero; if AnimateDome arrives first, rebuild it from the loaded
    // visual instead of cancelling the fade and leaving the world black.
    dome.animation = {dome.visual, queuedAnimation.target, eventTime,
                      queuedAnimation.duration, queuedAnimation.ease,
                      queuedAnimation.duration > 0.0f};
  } else {
    dome.animation.active = false;
  }
  ApplyDomeVisual(dome);
  std::string const url = ResolveMediaUrl(json);
  dome.media = ReadString(json, "media").value_or(url);
  dome.looping = ReadBool(json, "loop").value_or(false);
  dome.syncToSong = ReadBool(json, "syncToSong").value_or(true);
  dome.videoOffset = ReadFloat(json, "videoOffset").value_or(0.0f);
  dome.eventStartSongTime = eventTime;
  dome.authoredPlaybackSpeed = Clamp(ReadFloat(json, "speed").value_or(1.0f), 0.1f, 4.0f);
  float const realtime = UnityEngine::Time::get_realtimeSinceStartup();
  dome.prepareStartedRealtime =
      std::isfinite(realtime) ? std::max(realtime, 0.001f) : 0.001f;
  dome.playStartedRealtime = 0.0f;
  dome.prepareFailed = false;
  dome.textureBound = false;
  dome.safetyVisible = true;
  dome.pendingPlay = ReadBool(json, "autoplay").value_or(true);
  if (dome.pendingPlay) {
    dome.pendingInitialTime = dome.syncToSong
                                  ? 0.0
                                  : std::max(0.0, static_cast<double>(dome.videoOffset));
    dome.pendingInitialTimeTracksSong = dome.syncToSong;
  } else {
    dome.pendingInitialTime.reset();
    dome.pendingInitialTimeTracksSong = false;
  }
  dome.seekPending = false;
  dome.seekStartedRealtime = 0.0f;
  if (Alive(dome.material) && s_propVideoReady != 0) {
    dome.material->SetFloat(s_propVideoReady, 0.0f);
  }
  try {
    dome.video->set_url(StringW(url));
    dome.video->set_isLooping(dome.looping);
    dome.video->set_sendFrameReadyEvents(true);
    dome.video->Prepare();
  } catch (...) {
    FailVideo(dome);
    throw;
  }
  ApplyDomeVisual(dome);
  PaperLogger.info(
      "Nexora preparing '{}' on dome '{}' loop={} sync={} safetyBackdrop=visible",
      dome.media, dome.id, dome.looping, dome.syncToSong);
}

void Runtime::PlayVideo(DomeLayer& dome, rapidjson::Value const& json,
                        float eventTime) {
  if (!Alive(dome.video) || dome.prepareFailed) return;
  if (dome.media.empty()) {
    throw std::runtime_error("PlayVideo requires LoadVideo on the dome first");
  }
  dome.resumeAfterPause = false;
  dome.eventStartSongTime = ReadFloat(json, "eventStartSongTime").value_or(eventTime);
  dome.videoOffset = ReadFloat(json, "videoOffset").value_or(dome.videoOffset);
  dome.syncToSong = ReadBool(json, "syncToSong").value_or(dome.syncToSong);
  auto const explicitTime = ReadFloat(json, "time");
  double const desired = explicitTime.has_value()
                             ? static_cast<double>(*explicitTime)
                             : (dome.syncToSong
                                    ? std::max(
                                          0.0, static_cast<double>(SongTime()) -
                                                   dome.eventStartSongTime + dome.videoOffset)
                                    : static_cast<double>(dome.videoOffset));
  if (!std::isfinite(desired)) {
    throw std::runtime_error("PlayVideo produced a non-finite target time");
  }
  dome.pendingInitialTime = desired;
  dome.pendingInitialTimeTracksSong = !explicitTime.has_value() && dome.syncToSong;
  dome.pendingPlay = true;
  if (!dome.video->get_isPrepared()) {
    // Stop() releases decoder resources and clears isPrepared. Re-prepare the
    // existing local URL so an authored Stop -> Play sequence cannot wait
    // forever with no active decoder.
    dome.prepareFailed = false;
    dome.textureBound = false;
    dome.safetyVisible = true;
    float const realtime = UnityEngine::Time::get_realtimeSinceStartup();
    dome.prepareStartedRealtime =
        std::isfinite(realtime) ? std::max(realtime, 0.001f) : 0.001f;
    try {
      dome.video->set_sendFrameReadyEvents(true);
      dome.video->Prepare();
    } catch (...) {
      FailVideo(dome);
      throw;
    }
    ApplyDomeVisual(dome);
    return;
  }
  // UpdateVideo performs the seek and waits for seekCompleted (or its bounded
  // fallback) before the first Play. This prevents a stale frame at 0:00 when
  // practice mode or an authored Play event starts in the middle of a song.
}

void Runtime::PauseVideo(DomeLayer& dome) {
  if (!Alive(dome.video)) return;
  // This is an authored timeline pause, not an app/game suspension. Keeping
  // resumeAfterPause set here made a later Quest focus or pause-menu resume
  // restart video that the map explicitly asked Nexora to keep paused.
  dome.resumeAfterPause = false;
  if (dome.video->get_isPlaying()) dome.video->Pause();
  dome.pendingPlay = false;
}

void Runtime::StopVideo(DomeLayer& dome) {
  if (Alive(dome.video)) {
    dome.video->set_sendFrameReadyEvents(false);
    dome.video->Stop();
  }
  dome.pendingPlay = false;
  dome.resumeAfterPause = false;
  dome.pendingInitialTime.reset();
  dome.pendingInitialTimeTracksSong = false;
  dome.seekPending = false;
  dome.textureBound = false;
  dome.safetyVisible = false;
  dome.playStartedRealtime = 0.0f;
  dome.seekStartedRealtime = 0.0f;
  if (Alive(dome.material) && s_propVideoReady != 0) {
    dome.material->SetFloat(s_propVideoReady, 0.0f);
  }
  if (Alive(dome.renderer)) dome.renderer->set_enabled(false);
}

void Runtime::SeekVideo(DomeLayer& dome, rapidjson::Value const& json) {
  if (!Alive(dome.video) || dome.prepareFailed) return;
  auto const requested = ReadFloat(json, "time");
  if (!requested.has_value()) {
    throw std::runtime_error("SeekVideo requires a finite time");
  }
  dome.pendingInitialTime = std::max(0.0, static_cast<double>(*requested));
  dome.pendingInitialTimeTracksSong = false;
  if (!dome.video->get_isPrepared()) return;
  if (!dome.video->get_canSetTime()) {
    dome.pendingInitialTime.reset();
    PaperLogger.warn("Nexora decoder cannot seek dome '{}' on this source", dome.id);
    return;
  }
  double const normalized =
      NormalizeVideoTime(dome.video, *dome.pendingInitialTime, dome.looping);
  if (NeedsVideoSeek(dome.video, normalized)) {
    dome.video->set_time(normalized);
    dome.seekPending = true;
    float const realtime = UnityEngine::Time::get_realtimeSinceStartup();
    dome.seekStartedRealtime = std::isfinite(realtime) ? realtime : 0.0f;
  }
  dome.pendingInitialTime.reset();
}

void Runtime::SetPlayback(DomeLayer& dome, rapidjson::Value const& json) {
  dome.looping = ReadBool(json, "loop").value_or(dome.looping);
  dome.syncToSong = ReadBool(json, "syncToSong").value_or(dome.syncToSong);
  dome.videoOffset = ReadFloat(json, "videoOffset").value_or(dome.videoOffset);
  dome.authoredPlaybackSpeed = Clamp(
      ReadFloat(json, "speed").value_or(dome.authoredPlaybackSpeed), 0.1f, 4.0f);
  if (Alive(dome.video)) dome.video->set_isLooping(dome.looping);
}

void Runtime::ApplyDomeJson(DomeVisual& visual, DomeLayer& dome,
                            rapidjson::Value const& json) {
  auto assign = [&](char const* key, float& destination, float minimum, float maximum) {
    if (auto value = ReadFloat(json, key); value.has_value()) {
      destination = Clamp(*value, minimum, maximum);
    }
  };
  assign("radius", visual.radius, 2.0f, 500.0f);
  assign("opacity", visual.opacity, 0.0f, 1.0f);
  assign("brightness", visual.brightness, 0.0f, 8.0f);
  assign("exposure", visual.exposure, -4.0f, 4.0f);
  assign("saturation", visual.saturation, 0.0f, 3.0f);
  assign("hueShift", visual.hueShift, -1.0f, 1.0f);
  assign("yaw", visual.yaw, -3600.0f, 3600.0f);
  assign("pitch", visual.pitch, -3600.0f, 3600.0f);
  assign("roll", visual.roll, -3600.0f, 3600.0f);
  assign("scaleX", visual.scaleX, 0.05f, 8.0f);
  assign("scaleY", visual.scaleY, 0.05f, 8.0f);
  assign("scaleZ", visual.scaleZ, 0.05f, 8.0f);
  assign("deform", visual.deform, 0.0f, 1.0f);
  assign("deformFrequency", visual.deformFrequency, 0.1f, 64.0f);
  assign("deformSpeed", visual.deformSpeed, -20.0f, 20.0f);
  assign("ripple", visual.ripple, 0.0f, 1.0f);
  assign("rippleFrequency", visual.rippleFrequency, 0.1f, 64.0f);
  assign("rippleSpeed", visual.rippleSpeed, -20.0f, 20.0f);
  assign("twist", visual.twist, -3.0f, 3.0f);
  assign("pinch", visual.pinch, -1.0f, 1.0f);
  assign("pulse", visual.pulse, -0.8f, 2.0f);
  assign("kaleidoscope", visual.kaleidoscope, 0.0f, 16.0f);
  assign("pixelate", visual.pixelate, 0.0f, 1.0f);
  assign("chromatic", visual.chromatic, 0.0f, 0.25f);
  assign("scanline", visual.scanline, 0.0f, 1.0f);
  assign("vignette", visual.vignette, 0.0f, 1.0f);
  assign("fog", visual.fog, 0.0f, 1.0f);
  assign("flipX", visual.flipX, 0.0f, 1.0f);
  assign("flipY", visual.flipY, 0.0f, 1.0f);
  assign("swapEyes", visual.swapEyes, 0.0f, 1.0f);
  if (auto tint = ReadColor(json, "tint"); tint.has_value()) visual.tint = *tint;
  if (auto offset = ReadVector3(json, "offset"); offset.has_value()) dome.offset = *offset;
  dome.followPlayer = ReadBool(json, "followPlayer").value_or(dome.followPlayer);

  if (auto projection = ReadString(json, "projection"); projection.has_value()) {
    auto const normalizedProjection = LowerAscii(*projection);
    if (normalizedProjection == "topbottom" || normalizedProjection == "overunder" ||
        normalizedProjection == "tb" || normalizedProjection == "ou") {
      visual.projection = 1.0f;
    } else if (normalizedProjection == "sidebyside" || normalizedProjection == "sbs") {
      visual.projection = 2.0f;
    }
    else visual.projection = 0.0f;
  }
  if (Alive(dome.material)) {
    auto blend = ReadString(json, "blend").value_or("normal");
    if (blend == "additive") {
      dome.material->SetFloat(s_propSrcBlend, 5.0f);
      dome.material->SetFloat(s_propDstBlend, 1.0f);
    } else {
      dome.material->SetFloat(s_propSrcBlend, 5.0f);
      dome.material->SetFloat(s_propDstBlend, 10.0f);
    }
  }
}

void Runtime::ApplyDomeVisual(DomeLayer& dome) {
  if (!Alive(dome.material) || !Alive(dome.object)) return;
  auto const& value = dome.visual;

  // Cached Property ID setters (Zero String Allocations)
  dome.material->SetFloat(s_propOpacity, value.opacity);
  dome.material->SetFloat(s_propBrightness, value.brightness);
  dome.material->SetFloat(s_propExposure, value.exposure);
  dome.material->SetFloat(s_propSaturation, value.saturation);
  dome.material->SetFloat(s_propHueShift, value.hueShift);
  dome.material->SetFloat(s_propProjectionMode, value.projection);
  dome.material->SetFloat(s_propDeformAmplitude, value.deform);
  dome.material->SetFloat(s_propDeformFrequency, value.deformFrequency);
  dome.material->SetFloat(s_propDeformSpeed, value.deformSpeed);
  dome.material->SetFloat(s_propRippleAmount, value.ripple);
  dome.material->SetFloat(s_propRippleFrequency, value.rippleFrequency);
  dome.material->SetFloat(s_propRippleSpeed, value.rippleSpeed);
  dome.material->SetFloat(s_propTwist, value.twist);
  dome.material->SetFloat(s_propPinch, value.pinch);
  dome.material->SetFloat(s_propPulse, value.pulse);
  dome.material->SetFloat(s_propKaleidoscope, value.kaleidoscope);
  dome.material->SetFloat(s_propPixelate, value.pixelate);
  dome.material->SetFloat(s_propChromatic, value.chromatic);
  dome.material->SetFloat(s_propScanline, value.scanline);
  dome.material->SetFloat(s_propVignette, value.vignette);
  dome.material->SetFloat(s_propFog, value.fog);
  dome.material->SetFloat(s_propFlipX, value.flipX);
  dome.material->SetFloat(s_propFlipY, value.flipY);
  dome.material->SetFloat(s_propSwapEyes, value.swapEyes);
  dome.material->SetFloat(s_propVideoReady, dome.textureBound ? 1.0f : 0.0f);
  dome.material->SetColor(s_propTint, value.tint);

  auto const& camera = _cameraVisual;
  bool const cameraEnabled = GetCameraEffectsEnabled();
  dome.material->SetFloat(s_propCameraAmount, cameraEnabled ? camera.amount : 0.0f);
  dome.material->SetFloat(s_propCameraFisheye, camera.fisheye);
  dome.material->SetFloat(s_propCameraChromatic, camera.chromatic);
  dome.material->SetFloat(s_propCameraGlitch, camera.glitch);
  dome.material->SetFloat(s_propCameraVignette, camera.vignette);
  dome.material->SetFloat(s_propCameraScanline, camera.scanline);
  dome.material->SetFloat(s_propCameraPixelate, camera.pixelate);
  dome.material->SetFloat(s_propCameraGrayscale, camera.grayscale);
  dome.material->SetFloat(s_propCameraExposure, camera.exposure);
  dome.material->SetFloat(s_propCameraHueShift, camera.hueShift);
  dome.material->SetFloat(s_propCameraSplit, camera.split);
  dome.material->SetFloat(s_propCameraShake, camera.shake);
  dome.material->SetFloat(s_propCameraSwirl, camera.swirl);
  dome.material->SetFloat(s_propCameraKaleidoscope, camera.kaleidoscope);
  dome.material->SetColor(s_propCameraTint, camera.tint);

  auto transformReference = dome.object->get_transform();
  auto* transform = transformReference.unsafePtr();
  if (Alive(transform)) {
    transform->set_localScale(UnityEngine::Vector3(value.radius * value.scaleX,
                                                    value.radius * value.scaleY,
                                                    value.radius * value.scaleZ));
    transform->set_rotation(UnityEngine::Quaternion::Euler(value.pitch, value.yaw,
                                                            value.roll));
  }
  if (Alive(dome.renderer)) {
    bool const canRenderVideo =
        dome.textureBound && Alive(dome.video) &&
        dome.video->get_isPrepared() && value.opacity > 0.001f;
    bool const canRender =
        value.opacity > 0.001f && (dome.safetyVisible || canRenderVideo);
    dome.renderer->set_enabled(canRender);
  }
}

void Runtime::AnimateDome(DomeLayer& dome, rapidjson::Value const& json,
                          float eventTime) {
  DomeVisual target = dome.visual;
  ApplyDomeJson(target, dome, json);
  float const duration = Clamp(ReadFloat(json, "durationSeconds").value_or(1.0f),
                               0.0f, 120.0f);
  if (duration <= 0.0f) {
    dome.visual = target;
    dome.animation.active = false;
    ApplyDomeVisual(dome);
    return;
  }
  dome.animation = {dome.visual, target, eventTime, duration, ReadEase(json), true};
}

void Runtime::ApplyCameraJson(CameraVisual& visual, rapidjson::Value const& json) {
  auto assign = [&](char const* key, float& destination, float minimum, float maximum) {
    if (auto value = ReadFloat(json, key); value.has_value()) {
      destination = Clamp(*value, minimum, maximum);
    }
  };
  assign("amount", visual.amount, 0.0f, 1.0f);
  assign("fisheye", visual.fisheye, -1.0f, 1.0f);
  assign("chromatic", visual.chromatic, 0.0f, 0.2f);
  assign("glitch", visual.glitch, 0.0f, 1.0f);
  assign("vignette", visual.vignette, 0.0f, 1.0f);
  assign("scanline", visual.scanline, 0.0f, 1.0f);
  assign("pixelate", visual.pixelate, 0.0f, 1.0f);
  assign("grayscale", visual.grayscale, 0.0f, 1.0f);
  assign("exposure", visual.exposure, -4.0f, 4.0f);
  assign("hueShift", visual.hueShift, -1.0f, 1.0f);
  assign("split", visual.split, -0.3f, 0.3f);
  assign("shake", visual.shake, 0.0f, 0.2f);
  assign("swirl", visual.swirl, -2.0f, 2.0f);
  assign("kaleidoscope", visual.kaleidoscope, 0.0f, 16.0f);
  if (auto tint = ReadColor(json, "tint"); tint.has_value()) visual.tint = *tint;
}

void Runtime::SetCameraEffect(rapidjson::Value const& json, float eventTime,
                              bool animated) {
  if (!GetCameraEffectsEnabled()) return;
  CameraVisual target = _cameraVisual;
  ApplyCameraJson(target, json);
  float const duration = animated
                             ? Clamp(ReadFloat(json, "durationSeconds").value_or(1.0f),
                                     0.0f, 120.0f)
                             : 0.0f;
  if (duration <= 0.0f) {
    _cameraVisual = target;
    _cameraAnimation.active = false;
  } else {
    _cameraAnimation = {_cameraVisual, target, eventTime, duration, ReadEase(json), true};
  }
  if (target.amount > 0.001f || _cameraVisual.amount > 0.001f) {
    EnsureQuestSafeCameraEffects();
  }
}

void Runtime::EnsureQuestSafeCameraEffects() {
  if (_loggedQuestSafeCameraEffects) return;
  _loggedQuestSafeCameraEffects = true;
  PaperLogger.info(
      "Nexora camera events use the Quest-safe dome shader pass; no OnRenderImage framebuffer blit is attached");
}

void Runtime::UpdateCameraAnimation(float songTime) {
  if (_cameraAnimation.active) {
    float const raw = _cameraAnimation.duration <= 0.0f
                          ? 1.0f
                          : (songTime - _cameraAnimation.startSongTime) /
                                _cameraAnimation.duration;
    float const progress = EaseProgress(_cameraAnimation.ease, raw);
    _cameraVisual = LerpVisual(_cameraAnimation.start, _cameraAnimation.target, progress);
    if (raw >= 1.0f) {
      _cameraVisual = _cameraAnimation.target;
      _cameraAnimation.active = false;
    }
  }
}

float Runtime::SongTime() {
  if (_callbackController != nullptr) {
    float const songTime = _callbackController->get_songTime();
    return std::isfinite(songTime) ? songTime : 0.0f;
  }
  if (!Alive(_audioController)) {
    _audioController =
        UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  }
  if (!Alive(_audioController)) return 0.0f;
  float const songTime = _audioController->get_songTime();
  return std::isfinite(songTime) ? songTime : 0.0f;
}

float Runtime::TimeScale() {
  if (!Alive(_audioController)) {
    _audioController =
        UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  }
  if (!Alive(_audioController)) return 1.0f;
  float const timeScale = _audioController->get_timeScale();
  return std::isfinite(timeScale) ? Clamp(timeScale, 0.1f, 2.0f) : 1.0f;
}

void Runtime::FailVideo(DomeLayer& dome) {
  dome.prepareFailed = true;
  dome.pendingPlay = false;
  dome.pendingInitialTime.reset();
  dome.pendingInitialTimeTracksSong = false;
  dome.seekPending = false;
  dome.textureBound = false;
  dome.safetyVisible = true;
  dome.prepareStartedRealtime = 0.0f;
  dome.playStartedRealtime = 0.0f;
  dome.seekStartedRealtime = 0.0f;
  try {
    if (Alive(dome.video)) {
      dome.video->set_sendFrameReadyEvents(false);
      dome.video->Stop();
    }
  } catch (...) {
  }
  try {
    if (Alive(dome.material) && s_propVideoReady != 0) {
      dome.material->SetFloat(s_propVideoReady, 0.0f);
    }
    if (Alive(dome.renderer)) {
      dome.renderer->set_enabled(dome.visual.opacity > 0.001f);
    }
  } catch (...) {
  }
}

void Runtime::UpdateVideo(DomeLayer& dome, float songTime, float realtime) {
  if (!Alive(dome.video) || dome.prepareFailed) return;
  if (!std::isfinite(songTime) || !std::isfinite(realtime)) return;

  try {
    if (!dome.video->get_isPrepared()) {
      if (dome.prepareStartedRealtime > 0.0f &&
          realtime - dome.prepareStartedRealtime > GetPrepareTimeoutSeconds()) {
        PaperLogger.error(
            "Nexora Unity decoder timeout on '{}' after {}s; safety backdrop remains visible. Check adb logcat for AndroidVideoMedia.",
            dome.media, GetPrepareTimeoutSeconds());
        FailVideo(dome);
      }
      return;
    }

    if (dome.seekPending &&
        (dome.seekStartedRealtime <= 0.0f ||
         realtime - dome.seekStartedRealtime >= kSeekInFlightTimeoutSeconds)) {
      // seekCompleted is the primary signal. This timeout prevents one missed
      // platform callback from disabling song synchronization forever.
      dome.seekPending = false;
      dome.seekStartedRealtime = 0.0f;
    }

    bool initialSeek = false;
    double initialTarget = 0.0;
    bool const hasInitialTarget = dome.pendingInitialTime.has_value();
    if (hasInitialTarget) {
      initialTarget = dome.pendingInitialTimeTracksSong
                          ? std::max(0.0, static_cast<double>(songTime) -
                                              dome.eventStartSongTime + dome.videoOffset)
                          : *dome.pendingInitialTime;
    }

    if (hasInitialTarget && !dome.seekPending) {
      initialTarget = NormalizeVideoTime(dome.video, initialTarget, dome.looping);
      if (dome.video->get_canSetTime()) {
        initialSeek = NeedsVideoSeek(dome.video, initialTarget);
        if (initialSeek) {
          dome.video->set_time(initialTarget);
          dome.seekPending = true;
          dome.seekStartedRealtime = realtime;
        }
      } else if (dome.pendingInitialTime.has_value()) {
        PaperLogger.warn("Nexora decoder cannot seek dome '{}' on this source",
                         dome.id);
      }
      dome.pendingInitialTime.reset();
      dome.pendingInitialTimeTracksSong = false;
    }

    if (dome.pendingPlay && !dome.seekPending &&
        !dome.pendingInitialTime.has_value()) {
      dome.video->Play();
      float const speed = Clamp(dome.authoredPlaybackSpeed * TimeScale(), 0.1f, 4.0f);
      if (dome.video->get_canSetPlaybackSpeed()) {
        dome.video->set_playbackSpeed(speed);
      }
      dome.pendingPlay = false;
      dome.playStartedRealtime = realtime;
      PaperLogger.info(
          "Nexora started Unity playback on dome '{}' initialSeek={} target={:.3f}s speed={:.3f}",
          dome.id, initialSeek, initialTarget, speed);
    }

    if (!dome.textureBound && dome.playStartedRealtime > 0.0f &&
        realtime - dome.playStartedRealtime > GetPrepareTimeoutSeconds()) {
      PaperLogger.error(
          "Nexora Unity decoder produced no frameReady event within {}s on dome '{}' media='{}'; safety backdrop remains visible",
          GetPrepareTimeoutSeconds(), dome.id, dome.media);
      FailVideo(dome);
      return;
    }

    if (Alive(dome.renderer)) {
      dome.renderer->set_enabled(
          dome.visual.opacity > 0.001f &&
          (dome.safetyVisible ||
           (dome.textureBound && dome.video->get_isPrepared())));
    }

    if (!dome.video->get_isPlaying()) return;

    if (dome.video->get_canSetPlaybackSpeed()) {
      float const desiredSpeed =
          Clamp(dome.authoredPlaybackSpeed * TimeScale(), 0.1f, 4.0f);
      if (std::fabs(dome.video->get_playbackSpeed() - desiredSpeed) > 0.005f) {
        dome.video->set_playbackSpeed(desiredSpeed);
      }
    }

    if (!dome.syncToSong || !dome.video->get_canSetTime() || dome.seekPending ||
        realtime - dome.lastSyncRealtime < 0.5f) {
      return;
    }

    dome.lastSyncRealtime = realtime;
    double const desired = NormalizeVideoTime(
        dome.video,
        static_cast<double>(songTime) - dome.eventStartSongTime + dome.videoOffset,
        dome.looping);
    double const current = dome.video->get_time();
    double const drift =
        std::isfinite(current) ? std::fabs(current - desired)
                               : std::numeric_limits<double>::infinity();
    if (drift > GetSyncToleranceSeconds()) {
      dome.video->set_time(desired);
      dome.seekPending = true;
      dome.seekStartedRealtime = realtime;
      if (GetDebugLoggingEnabled()) {
        PaperLogger.info("Nexora resync dome '{}': drift={:.3f}s target={:.3f}s",
                         dome.id, drift, desired);
      }
    }
  } catch (std::exception const& ex) {
    PaperLogger.error("Nexora UpdateVideo exception on dome '{}': {}", dome.id, ex.what());
    FailVideo(dome);
  } catch (...) {
    PaperLogger.error("Nexora UpdateVideo non-standard exception on dome '{}'", dome.id);
    FailVideo(dome);
  }
}

void Runtime::OnVideoFrameReady(UnityEngine::Video::VideoPlayer* player,
                                std::int64_t frameIndex) {
  try {
    if (!Alive(player) || frameIndex < 0 || !player->get_isPrepared()) return;

    for (auto& [_, dome] : _domes) {
      if (dome.video != player || dome.prepareFailed) continue;

      auto textureReference = player->get_texture();
      auto* texture = textureReference.unsafePtr();
      if (!Alive(texture)) {
        PaperLogger.warn(
            "Nexora received frameReady={} for dome '{}' without a Unity texture; keeping safety backdrop",
            frameIndex, dome.id);
        return;
      }

      bool const firstFrame = !dome.textureBound;
      dome.textureBound = true;
      dome.safetyVisible = false;
      dome.playStartedRealtime = 0.0f;
      if (Alive(dome.material)) {
        if (s_propMainTex != 0) dome.material->SetTexture(s_propMainTex, texture);
        if (s_propVideoReady != 0) dome.material->SetFloat(s_propVideoReady, 1.0f);
      }
      if (Alive(dome.renderer)) {
        dome.renderer->set_enabled(dome.visual.opacity > 0.001f);
      }

      // MaterialOverride keeps updating the decoder-owned texture. One verified
      // callback is enough for the visibility gate and avoids 60 managed calls
      // per second for 4K60 map media.
      player->set_sendFrameReadyEvents(false);
      if (firstFrame) {
        PaperLogger.info(
            "Nexora frameReady revealed dome '{}' at decoded frame {} pipeline=UnityMaterialOverride",
            dome.id, frameIndex);
      }
      return;
    }
  } catch (std::exception const& exception) {
    PaperLogger.error("Nexora frameReady callback failed safely: {}",
                      exception.what());
  } catch (...) {
    PaperLogger.error("Nexora frameReady callback failed safely");
  }
  for (auto& [_, dome] : _domes) {
    if (dome.video == player) {
      FailVideo(dome);
      return;
    }
  }
}

void Runtime::OnVideoPrepared(UnityEngine::Video::VideoPlayer* player) {
  try {
    if (!Alive(player) || !player->get_isPrepared()) return;
    for (auto& [_, dome] : _domes) {
      if (dome.video != player) continue;
      auto const width = player->get_width();
      auto const height = player->get_height();
      double const length = player->get_length();
      if (width == 0 || height == 0 || !std::isfinite(length) || length <= 0.001) {
        PaperLogger.error(
            "Nexora decoder prepared invalid media metadata on dome '{}' size={}x{} length={}",
            dome.id, width, height, length);
        FailVideo(dome);
        return;
      }
      if (player->get_canSetSkipOnDrop()) player->set_skipOnDrop(true);
      dome.prepareStartedRealtime = 0.0f;
      PaperLogger.info(
          "Nexora Unity decoder prepared dome '{}' media='{}' size={}x{} length={:.3f}s",
          dome.id, dome.media, width, height, length);
      return;
    }
  } catch (std::exception const& exception) {
    PaperLogger.error("Nexora prepareCompleted callback failed safely: {}",
                      exception.what());
  } catch (...) {
    PaperLogger.error("Nexora prepareCompleted callback failed safely");
  }
  for (auto& [_, dome] : _domes) {
    if (dome.video == player) {
      FailVideo(dome);
      return;
    }
  }
}

void Runtime::OnVideoSeekCompleted(UnityEngine::Video::VideoPlayer* player) {
  try {
    for (auto& [_, dome] : _domes) {
      if (dome.video != player) continue;
      dome.seekPending = false;
      dome.seekStartedRealtime = 0.0f;
      return;
    }
  } catch (...) {
    PaperLogger.error("Nexora seekCompleted callback failed safely");
  }
}

void Runtime::OnVideoError(UnityEngine::Video::VideoPlayer* player,
                           StringW message) {
  std::string const safeMessage = SafeManagedString(message);
  try {
    for (auto& [_, dome] : _domes) {
      if (dome.video != player) continue;
      FailVideo(dome);
      PaperLogger.error(
          "Nexora Unity decoder error on dome '{}' media='{}': {}; safety backdrop remains visible",
          dome.id, dome.media, safeMessage);
      return;
    }
    PaperLogger.error("Nexora Unity decoder error after dome retirement: {}",
                      safeMessage);
  } catch (...) {
    PaperLogger.error("Nexora errorReceived callback failed safely: {}", safeMessage);
  }
}

void Runtime::UpdateDomes(float songTime) {
  auto cameraReference = UnityEngine::Camera::get_main();
  auto* camera = cameraReference.unsafePtr();
  UnityEngine::Vector3 cameraPosition = UnityEngine::Vector3::get_zero();
  if (Alive(camera)) {
    auto cameraTransform = camera->get_transform();
    if (Alive(cameraTransform.unsafePtr())) {
      auto const position = cameraTransform->get_position();
      if (std::isfinite(position.x) && std::isfinite(position.y) &&
          std::isfinite(position.z)) {
        cameraPosition = position;
      }
    }
  }
  float const rawRealtime = UnityEngine::Time::get_realtimeSinceStartup();
  float const realtime = std::isfinite(rawRealtime) ? rawRealtime : 0.0f;
  std::vector<std::string> brokenDomes;
  for (auto& [id, dome] : _domes) {
    try {
      if (dome.animation.active) {
        float const raw = dome.animation.duration <= 0.0f
                              ? 1.0f
                              : (songTime - dome.animation.startSongTime) /
                                    dome.animation.duration;
        dome.visual = LerpVisual(dome.animation.start, dome.animation.target,
                                 EaseProgress(dome.animation.ease, raw));
        if (raw >= 1.0f) {
          dome.visual = dome.animation.target;
          dome.animation.active = false;
        }
      }
      if (Alive(dome.object)) {
        auto transformReference = dome.object->get_transform();
        auto* transform = transformReference.unsafePtr();
        if (Alive(transform)) {
          auto base =
              dome.followPlayer ? cameraPosition : UnityEngine::Vector3::get_zero();
          transform->set_position(UnityEngine::Vector3(base.x + dome.offset.x,
                                                        base.y + dome.offset.y,
                                                        base.z + dome.offset.z));
        }
      }
      UpdateVideo(dome, songTime, realtime);
      ApplyDomeVisual(dome);
    } catch (std::exception const& exception) {
      PaperLogger.error("Nexora retiring broken dome '{}': {}", id,
                        exception.what());
      brokenDomes.push_back(id);
    } catch (...) {
      PaperLogger.error("Nexora retiring broken dome '{}' after a non-standard exception",
                        id);
      brokenDomes.push_back(id);
    }
  }
  for (auto const& id : brokenDomes) DestroyDome(id);
}

void Runtime::Update() {
  try {
    if (_pendingReset && _lifecycle.RenderDepth() == 0) FinishPendingReset();
    if (_currentBeatmapData == nullptr) TryPrepareSelectedBeatmapFromScene();
    if (!_lifecycle.IsActive() && !_lifecycle.IsSuspended()) return;

    auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (scene.get_name() == "MainMenu") {
      ResetSession(true);
      return;
    }
    if (_lifecycle.IsSuspended()) return;
    float const songTime = SongTime();
    if (_lastSongTime >= 0.0f && songTime + 0.35f < _lastSongTime) {
      for (auto& [_, dome] : _domes) dome.lastSyncRealtime = -1000.0f;
    }
    _lastSongTime = songTime;
    UpdateCameraAnimation(songTime);
    // Camera-event values are shader inputs on every Nexora dome. Update them
    // before the dome pass so there is no one-frame event delay.
    UpdateDomes(songTime);
  } catch (std::exception const& exception) {
    int const frame = UnityEngine::Time::get_frameCount();
    if (frame - _lastUpdateErrorFrame >= 90) {
      _lastUpdateErrorFrame = frame;
      PaperLogger.error("Nexora update skipped a failing frame: {}", exception.what());
    }
  } catch (...) {
    int const frame = UnityEngine::Time::get_frameCount();
    if (frame - _lastUpdateErrorFrame >= 90) {
      _lastUpdateErrorFrame = frame;
      PaperLogger.error("Nexora update skipped a non-standard exception");
    }
  }
}

void Runtime::SetPaused(bool paused) {
  _paused = paused;
  ApplyPauseState();
}

void Runtime::SetApplicationPaused(bool paused) {
  _applicationPaused = paused;
  ApplyPauseState();
}

void Runtime::ApplyPauseState() {
  bool const suspend = _paused || _applicationPaused || !_focused;
  if (suspend) {
    _lifecycle.Suspend();
    for (auto& [_, dome] : _domes) {
      try {
        if (!Alive(dome.video)) continue;
        bool const wasPlaying = dome.video->get_isPlaying();
        dome.resumeAfterPause = dome.resumeAfterPause || wasPlaying;
        if (wasPlaying) dome.video->Pause();
      } catch (...) {
        PaperLogger.warn("Nexora could not pause decoder for dome '{}'", dome.id);
      }
    }
  } else {
    _lifecycle.Resume();
    float const rawRealtime = UnityEngine::Time::get_realtimeSinceStartup();
    float const resumedRealtime = std::isfinite(rawRealtime) ? rawRealtime : 0.0f;
    for (auto& [_, dome] : _domes) {
      try {
        // realtimeSinceStartup keeps advancing while the headset is suspended.
        // Give an in-flight Android decoder a fresh preparation window after
        // focus returns instead of treating time spent in the Quest shell or
        // pause menu as a decoder timeout.
        if (Alive(dome.video) && !dome.video->get_isPrepared() &&
            !dome.prepareFailed && dome.prepareStartedRealtime > 0.0f) {
          dome.prepareStartedRealtime = resumedRealtime;
        }
        if (Alive(dome.video) && dome.resumeAfterPause &&
            dome.video->get_isPrepared()) {
          dome.video->Play();
          if (!dome.textureBound) {
            dome.playStartedRealtime = resumedRealtime;
          }
        }
      } catch (...) {
        PaperLogger.warn("Nexora could not resume decoder for dome '{}'", dome.id);
      }
      dome.resumeAfterPause = false;
      dome.lastSyncRealtime = -1000.0f;
    }
  }
}

void Runtime::SetFocused(bool focused) {
  _focused = focused;
  ApplyPauseState();
}

void Runtime::DestroyDome(std::string const& id, bool canTouchUnity) {
  auto iterator = _domes.find(id);
  if (iterator == _domes.end()) return;
  auto layer = iterator->second;
  _domes.erase(iterator);
  if (!canTouchUnity) return;
  try {
    if (Alive(layer.video)) {
      if (layer.frameReadyDelegate != nullptr) {
        layer.video->remove_frameReady(layer.frameReadyDelegate);
      }
      if (layer.prepareCompletedDelegate != nullptr) {
        layer.video->remove_prepareCompleted(layer.prepareCompletedDelegate);
      }
      if (layer.seekCompletedDelegate != nullptr) {
        layer.video->remove_seekCompleted(layer.seekCompletedDelegate);
      }
      if (layer.errorReceivedDelegate != nullptr) {
        layer.video->remove_errorReceived(layer.errorReceivedDelegate);
      }
      layer.video->set_sendFrameReadyEvents(false);
      layer.video->set_targetMaterialRenderer(nullptr);
      layer.video->Stop();
    }
  } catch (...) {
  }
  try {
    if (Alive(layer.material)) UnityEngine::Object::Destroy(layer.material);
    if (Alive(layer.mesh)) UnityEngine::Object::Destroy(layer.mesh);
    if (Alive(layer.object)) UnityEngine::Object::Destroy(layer.object);
  } catch (...) {
    PaperLogger.warn("Nexora caught exception while destroying dome '{}'", id);
  }
}

void Runtime::DestroyAllDomes(bool canTouchUnity) {
  if (!canTouchUnity) {
    _domes.clear();
    return;
  }
  while (!_domes.empty()) DestroyDome(_domes.begin()->first, true);
}

void Runtime::ResetSession(bool sceneTransition) {
  if (!_lifecycle.IsActive() && !_lifecycle.IsSuspended() && !_lifecycle.IsRetiring() &&
      _domes.empty() && _callbackController == nullptr && _currentBeatmapData == nullptr) {
    return;
  }
  [[maybe_unused]] auto const retirementGeneration = _lifecycle.BeginRetirement();
  if (_lifecycle.RenderDepth() != 0) {
    _pendingReset = true;
    _pendingResetSceneTransition = _pendingResetSceneTransition || sceneTransition;
    return;
  }
  bool const canTouchUnity = !sceneTransition;
  DestroyAllDomes(canTouchUnity);
  _callbackController = nullptr;
  _currentBeatmapData = nullptr;
  _processedNexoraEvents.clear();
  _preparingBeatmap = false;
  _audioController = nullptr;
  _cameraVisual = {};
  _cameraAnimation.active = false;
  _nextGameplayProbeFrame = -1;
  _lastSongTime = -1.0f;
  _pendingReset = false;
  _pendingResetSceneTransition = false;
  [[maybe_unused]] bool const retirementCompleted = _lifecycle.CompleteRetirement();
}

void Runtime::FinishPendingReset() {
  bool const sceneTransition = _pendingResetSceneTransition;
  _pendingReset = false;
  _pendingResetSceneTransition = false;
  ResetSession(sceneTransition);
}

void Runtime::HandleScenesWillDismiss() {
  // This hook runs before GameScenesManager returns its transition coroutine,
  // so the gameplay objects and decoder delegates are still valid to detach.
  ResetSession(false);
}

void Runtime::HandleGameplayRestart() { ResetSession(false); }

void Runtime::OnBehaviourDestroyed(RuntimeBehaviour* behaviour) {
  if (_behaviour == behaviour) _behaviour = nullptr;
}

}  // namespace Nexora
