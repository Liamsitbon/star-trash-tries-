#include "NexoraRuntime.hpp"

#include "NexoraComponents.hpp"
#include "QuestInterop.hpp"
#include "main.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include <unistd.h>

#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
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

constexpr std::string_view kAssetsPath =
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/Nexora/Assets/nexoraassets.android";
constexpr std::string_view kAssetsAlternatePath =
    "/storage/emulated/0/ModData/com.beatgames.beatsaber/Mods/Nexora/Assets/nexoraassets.android";
constexpr std::string_view kDomeMaterialAsset = "assets/nexora/materials/nexoradome.mat";
constexpr int kVideoTextureWidth = 2048;
constexpr int kVideoTextureHeight = 1024;

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
int s_propCull = 0;
int s_propFlipX = 0;
int s_propFlipY = 0;
int s_propSwapEyes = 0;

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
  if (iterator->value.IsNumber()) return iterator->value.GetFloat();
  if (iterator->value.IsBool()) return iterator->value.GetBool() ? 1.0f : 0.0f;
  return std::nullopt;
}

std::optional<bool> ReadBool(rapidjson::Value const& object, char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd()) return std::nullopt;
  if (iterator->value.IsBool()) return iterator->value.GetBool();
  if (iterator->value.IsNumber()) return iterator->value.GetFloat() != 0.0f;
  return std::nullopt;
}

std::optional<std::string> ReadString(rapidjson::Value const& object, char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsString()) return std::nullopt;
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
  return UnityEngine::Vector3(array[0].GetFloat(), array[1].GetFloat(),
                              array[2].GetFloat());
}

std::optional<UnityEngine::Color> ReadColor(rapidjson::Value const& object,
                                            char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsArray() ||
      iterator->value.Size() < 3 || iterator->value.Size() > 4) {
    return std::nullopt;
  }
  auto array = iterator->value.GetArray();
  for (auto const& value : array) {
    if (!value.IsNumber()) return std::nullopt;
  }
  return UnityEngine::Color(array[0].GetFloat(), array[1].GetFloat(),
                            array[2].GetFloat(),
                            array.Size() == 4 ? array[3].GetFloat() : 1.0f);
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
  _propertyIdsInitialized = true;

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
  s_propCull = UnityEngine::Shader::PropertyToID(StringW("_Cull"));
  s_propFlipX = UnityEngine::Shader::PropertyToID(StringW("_FlipX"));
  s_propFlipY = UnityEngine::Shader::PropertyToID(StringW("_FlipY"));
  s_propSwapEyes = UnityEngine::Shader::PropertyToID(StringW("_SwapEyes"));

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
  mesh->set_name(StringW("NexoraInwardDomeMesh"));
  mesh->set_vertices(vertices);
  mesh->set_uv(uvs);
  mesh->set_normals(normals);
  mesh->set_triangles(triangles);
  mesh->RecalculateBounds();
  return mesh;
}

UnityEngine::Shader* Runtime::FindUsableShader() {
  static char const* const candidateShaderNames[] = {
      "Unlit/Texture",
      "BeatSaber/UnlitGlow"
  };
  for (char const* name : candidateShaderNames) {
    auto ref = UnityEngine::Shader::Find(StringW(name));
    auto* s = ref.unsafePtr();
    if (Alive(s) && s->get_isSupported()) {
      PaperLogger.info("Nexora using fallback shader: '{}'", name);
      return s;
    }
  }
  return nullptr;
}

void Runtime::LateLoad() {
  if (GetNexoraEnabled()) {
    EnsureBehaviour();
    LoadAssets();
    if (!SongCore::API::Capabilities::IsCapabilityRegistered(kCapability)) {
      SongCore::API::Capabilities::RegisterCapability(kCapability);
    }
  } else if (SongCore::API::Capabilities::IsCapabilityRegistered(kCapability)) {
    SongCore::API::Capabilities::UnregisterCapability(kCapability);
  }
  CustomJSONData::CustomEventCallbacks::AddCustomEventCallback(&Runtime::OnCustomEventStatic);
  SongCore::API::LevelSelect::GetLevelWasSelectedEvent() +=
      [](SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
        std::string mapRoot;
        if (event.isCustom && event.customBeatmapLevel != nullptr) {
          mapRoot = std::string(event.customBeatmapLevel->customLevelPath);
        }
        bool requiresNexora = false;
        if (event.isCustom && event.customLevelDetails) {
          auto const& requirements = event.customLevelDetails->difficultyDetails.requirements;
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
        auto& runtime = Runtime::Instance();
        runtime.SetSelectedMapRoot(std::move(mapRoot), requiresNexora);

        // Capability registration tells SongCore the mod is installed. Only
        // enable the play button after a required map has passed the private
        // Quest-shader readiness check; enabling it first lets a map launch
        // into the exact black/fallback state this guard is meant to prevent.
        if (!requiresNexora) {
          SongCore::API::PlayButton::EnablePlayButton(std::string(kCapability));
          return;
        }
        if (!GetNexoraEnabled()) {
          SongCore::API::PlayButton::DisablePlayButton(
              std::string(kCapability),
              "Nexora is disabled in its Quest configuration.");
          return;
        }

        // fileCopies can complete after late-load on some installers. Retry
        // only for maps that actually require Nexora; ordinary SongCore menu
        // selection must not touch Nexora's Unity asset state.
        runtime.LoadAssets();
        if (!runtime.HasQuestShaderAssets()) {
          std::string const failure = runtime.QuestShaderAssetFailure();
          PaperLogger.error("Nexora required-map readiness failed: {}", failure);
          SongCore::API::PlayButton::DisablePlayButton(
              std::string(kCapability), failure);
          return;
        }
        SongCore::API::PlayButton::EnablePlayButton(std::string(kCapability));
      };
  PaperLogger.info(
      "Nexora runtime ready; media is map-embedded and Vivify bundles are never loaded");
}

bool Runtime::HasQuestShaderAssets() const {
  return Alive(_assetBundle) && Alive(_domeShader) &&
         _domeShader->get_name() == u"Nexora/VideoDome" &&
         _domeShader->get_isSupported();
}

std::string Runtime::QuestShaderAssetFailure() const {
  bool filePresent = false;
  for (auto const path : {kAssetsPath, kAssetsAlternatePath}) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(std::string(path), ec) ||
        access(std::string(path).c_str(), R_OK) == 0) {
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
  try {
    auto* gameObject = UnityEngine::GameObject::New_ctor(StringW("NexoraRuntime"));
    if (Alive(gameObject)) {
      UnityEngine::Object::DontDestroyOnLoad(gameObject);
      _behaviour = gameObject->AddComponent<RuntimeBehaviour*>();
    }
  } catch (...) {
    PaperLogger.warn("Nexora: EnsureBehaviour could not instantiate yet (will retry in session start)");
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
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec) || access(path.c_str(), R_OK) == 0) {
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
  Runtime::Instance().HandleCustomEvent(callbackController, customEventData);
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
  ReplayMissedEvents(std::max(0.0f, triggerTime) + 0.075f);
  _preparingBeatmap = false;
  PaperLogger.info(
      "Nexora PrepareBeatmap source={} customEvents={} replayed={} triggerTime={:.3f}",
      source, customBeatmapData->customEventDatas.size(),
      _processedNexoraEvents.size(), triggerTime);
  return true;
}

void Runtime::ReplayMissedEvents(float upToTime) {
  if (_currentBeatmapData == nullptr || _callbackController == nullptr) return;
  for (auto* eventData : _currentBeatmapData->customEventDatas) {
    if (eventData == nullptr || eventData->time > upToTime ||
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
  _lastSongTime = callbackController != nullptr ? callbackController->get_songTime() : 0.0f;
  _paused = false;
  LoadAssets();
  PaperLogger.info("Nexora gameplay session {} started", _sessionGeneration);
}

std::string Runtime::DomeId(rapidjson::Value const& json) const {
  return ReadString(json, "id").value_or("main");
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
  if (!_processedNexoraEvents.emplace(customEventData).second) return;

  auto const* json = EventJson(customEventData);
  if (json == nullptr || !json->IsObject()) {
    PaperLogger.warn("Nexora ignored event '{}' at {:.3f}: data is not an object",
                     std::string(customEventData->type), customEventData->time);
    return;
  }

  try {
    BeginSession(callbackController);
    std::string_view const type = customEventData->type;
    float const eventTime = customEventData->time;

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
      burst.amount = ReadFloat(*json, "amount").value_or(1.0f);
      burst.glitch = ReadFloat(*json, "glitch").value_or(1.0f);
      burst.chromatic = ReadFloat(*json, "chromatic").value_or(0.18f);
      burst.split = ReadFloat(*json, "split").value_or(0.12f);
      _cameraVisual = burst;
      _cameraAnimation = {burst, target, eventTime,
                          Clamp(ReadFloat(*json, "durationSeconds").value_or(0.35f),
                                0.02f, 30.0f),
                          Ease::OutCubic, true};
      EnsureQuestSafeCameraEffects();
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
    } else if (type == kTransitionEvent) {
      float const opacity = Clamp(ReadFloat(*json, "opacity").value_or(0.0f), 0.0f, 1.0f);
      float const duration = Clamp(ReadFloat(*json, "durationSeconds").value_or(1.0f),
                                   0.0f, 120.0f);
      for (auto& [_, layer] : _domes) {
        DomeVisual target = layer.visual;
        target.opacity = opacity;
        layer.animation = {layer.visual, target, eventTime, duration, ReadEase(*json),
                           duration > 0.0f};
        if (duration <= 0.0f) layer.visual = target;
      }
    } else if (type == kPulseEvent || type == kShockwaveEvent) {
      DomeVisual target = dome->visual;
      DomeVisual burst = target;
      if (type == kPulseEvent) {
        burst.pulse = ReadFloat(*json, "pulse").value_or(0.18f);
        burst.brightness = ReadFloat(*json, "brightness").value_or(
            std::max(1.0f, target.brightness * 1.25f));
      } else {
        burst.ripple = ReadFloat(*json, "ripple").value_or(0.15f);
        burst.rippleFrequency = ReadFloat(*json, "rippleFrequency").value_or(12.0f);
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

  auto* object = UnityEngine::GameObject::New_ctor(StringW("NexoraDome_" + id));
  if (!Alive(object)) throw std::runtime_error("Unity could not create the 360 dome GameObject");

  auto* filter = object->AddComponent<UnityEngine::MeshFilter*>();
  auto* renderer = object->AddComponent<UnityEngine::MeshRenderer*>();
  if (!Alive(filter) || !Alive(renderer)) {
    UnityEngine::Object::Destroy(object);
    throw std::runtime_error("Nexora dome failed to attach MeshFilter/MeshRenderer");
  }

  int const resolution = GetDomeResolution();
  auto* mesh = CreateProceduralDomeMesh(resolution, resolution, 1.0f);
  filter->set_sharedMesh(mesh);

  UnityEngine::Material* material = nullptr;
  bool customShader = false;
  if (!GetForceUnlitFallback() && Alive(_domeShader) && _domeShader->get_isSupported()) {
    // Clone the validated bundle material so its serialized render state and
    // Quest shader variant selection are preserved exactly.
    material = Alive(_domeTemplate)
                   ? UnityEngine::Material::New_ctor(_domeTemplate.ptr())
                   : UnityEngine::Material::New_ctor(_domeShader.ptr());
    customShader = Alive(material);
  }
  if (!Alive(material)) {
    auto* fallbackShader = FindUsableShader();
    if (Alive(fallbackShader)) {
      material = UnityEngine::Material::New_ctor(fallbackShader);
      PaperLogger.warn(
          "Nexora dome '{}' is using a basic mono fallback; stereo and authored effects are unavailable",
          id);
    }
  }
  if (!Alive(material)) {
    UnityEngine::Object::Destroy(object);
    throw std::runtime_error("Nexora could not create dome material or fallback");
  }

  material->set_renderQueue(1001);
  if (s_propCull != 0) material->SetFloat(s_propCull, 0.0f);
  renderer->set_sharedMaterial(material);
  renderer->set_receiveShadows(false);
  renderer->set_enabled(false);

  auto* video = object->AddComponent<UnityEngine::Video::VideoPlayer*>();
  if (!Alive(video)) {
    UnityEngine::Object::Destroy(material);
    UnityEngine::Object::Destroy(object);
    throw std::runtime_error("Unity VideoPlayer component is unavailable");
  }
  video->set_source(UnityEngine::Video::VideoSource::Url);

  // Cinema's reliable path is a decoder-owned RenderTexture, not a direct
  // external texture sampled from VideoPlayer.texture. The latter can report
  // prepared on Android while the custom dome keeps sampling its black
  // placeholder. A 2:1 2048 texture is enough for the Quest display and keeps
  // the allocation bounded even when the authored source is 4K60.
  UnityEngine::RenderTexture* videoTexture = nullptr;
  try {
    videoTexture = UnityEngine::RenderTexture::New_ctor(
        kVideoTextureWidth, kVideoTextureHeight, 0,
        UnityEngine::RenderTextureFormat::ARGB32);
    if (Alive(videoTexture)) {
      videoTexture->set_name(StringW("NexoraVideoTexture_" + id));
      videoTexture->set_filterMode(UnityEngine::FilterMode::Bilinear);
      videoTexture->set_wrapMode(UnityEngine::TextureWrapMode::Repeat);
      if (!videoTexture->Create()) {
        UnityEngine::Object::Destroy(videoTexture);
        videoTexture = nullptr;
      }
    }
  } catch (...) {
    if (Alive(videoTexture)) UnityEngine::Object::Destroy(videoTexture);
    videoTexture = nullptr;
  }
  bool const renderTexturePipeline = Alive(videoTexture);
  if (renderTexturePipeline) {
    video->set_renderMode(UnityEngine::Video::VideoRenderMode::RenderTexture);
    video->set_targetTexture(videoTexture);
    if (s_propMainTex != 0) material->SetTexture(s_propMainTex, videoTexture);
  } else {
    // Keep a functional low-allocation fallback for devices that reject the
    // RenderTexture. It remains hidden until an actual decoded frame exists.
    video->set_renderMode(UnityEngine::Video::VideoRenderMode::APIOnly);
    PaperLogger.warn("Nexora dome '{}' could not allocate the Cinema-style video texture; using APIOnly fallback", id);
  }

  video->set_audioOutputMode(UnityEngine::Video::VideoAudioOutputMode::None);
  video->set_playOnAwake(false);
  video->set_waitForFirstFrame(true);
  std::function<void(UnityEngine::Video::VideoPlayer*, std::int64_t)> frameReady =
      [](UnityEngine::Video::VideoPlayer* player, std::int64_t frameIndex) {
        Runtime::Instance().OnVideoFrameReady(player, frameIndex);
      };
  auto* frameReadyDelegate =
      custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_FrameReadyEventHandler*>(
          frameReady);
  video->add_frameReady(frameReadyDelegate);
  video->set_sendFrameReadyEvents(true);

  std::function<void(UnityEngine::Video::VideoPlayer*)> prepareCompleted =
      [](UnityEngine::Video::VideoPlayer* player) {
        Runtime::Instance().OnVideoPrepared(player);
      };
  auto* prepareCompletedDelegate =
      custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_EventHandler*>(
          prepareCompleted);
  video->add_prepareCompleted(prepareCompletedDelegate);

  std::function<void(UnityEngine::Video::VideoPlayer*, StringW)> errorReceived =
      [](UnityEngine::Video::VideoPlayer* player, StringW message) {
        Runtime::Instance().OnVideoError(player, message);
      };
  auto* errorReceivedDelegate =
      custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_ErrorEventHandler*>(
          errorReceived);
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
  layer.errorReceivedDelegate = errorReceivedDelegate;
  layer.videoTexture = videoTexture;
  layer.customShader = customShader;
  layer.renderTexturePipeline = renderTexturePipeline;
  auto [iterator, inserted] = _domes.emplace(id, std::move(layer));
  ApplyDomeVisual(iterator->second);
  PaperLogger.info("Nexora created procedural dome '{}' (res={} videoPipeline={} videoTexture={}x{} layers={}/{})",
                   id, resolution, renderTexturePipeline ? "RenderTexture" : "APIOnly",
                   renderTexturePipeline ? kVideoTextureWidth : 0,
                   renderTexturePipeline ? kVideoTextureHeight : 0,
                   _domes.size(), GetMaxLayers());
  return &iterator->second;
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
    if (ec || !IsPathInside(root, candidate) ||
        !std::filesystem::is_regular_file(candidate, ec)) {
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
  dome.prepareStartedRealtime = UnityEngine::Time::get_realtimeSinceStartup();
  dome.prepareFailed = false;
  dome.textureBound = false;
  dome.pendingPlay = ReadBool(json, "autoplay").value_or(true);
  dome.video->set_url(StringW(url));
  dome.video->set_isLooping(dome.looping);
  // Unity disables these notifications if a previous source completed. Enable
  // them for every authored load so the dome is revealed only after the new
  // source has produced a real decoded frame.
  dome.video->set_sendFrameReadyEvents(true);
  if (dome.video->get_canSetSkipOnDrop()) dome.video->set_skipOnDrop(true);
  dome.video->Prepare();
  if (Alive(dome.renderer)) dome.renderer->set_enabled(false);
  PaperLogger.info("Nexora preparing '{}' on dome '{}' loop={} sync={}", dome.media,
                   dome.id, dome.looping, dome.syncToSong);
}

void Runtime::PlayVideo(DomeLayer& dome, rapidjson::Value const& json,
                        float eventTime) {
  if (!Alive(dome.video)) return;
  dome.resumeAfterPause = false;
  dome.eventStartSongTime = ReadFloat(json, "eventStartSongTime").value_or(eventTime);
  dome.videoOffset = ReadFloat(json, "videoOffset").value_or(dome.videoOffset);
  dome.syncToSong = ReadBool(json, "syncToSong").value_or(dome.syncToSong);
  if (!dome.video->get_isPrepared()) {
    dome.pendingPlay = true;
    return;
  }
  if (dome.video->get_canSetTime()) {
    double desired = ReadFloat(json, "time").value_or(
        dome.syncToSong ? std::max(0.0f, SongTime() - dome.eventStartSongTime + dome.videoOffset)
                        : dome.videoOffset);
    dome.video->set_time(std::max(0.0, desired));
  }
  // Seek before Play so Quest never flashes frame zero while joining a song
  // or resuming a preloaded layer at an authored offset.
  dome.video->Play();
  dome.pendingPlay = false;
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
  if (Alive(dome.video)) dome.video->Stop();
  dome.pendingPlay = false;
  dome.resumeAfterPause = false;
  dome.textureBound = false;
  if (Alive(dome.renderer)) dome.renderer->set_enabled(false);
}

void Runtime::SeekVideo(DomeLayer& dome, rapidjson::Value const& json) {
  if (!Alive(dome.video) || !dome.video->get_isPrepared() ||
      !dome.video->get_canSetTime()) {
    return;
  }
  double const time = std::max(0.0f, ReadFloat(json, "time").value_or(0.0f));
  dome.video->set_time(time);
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
  bool const canRender =
      dome.textureBound &&
      Alive(dome.video) &&
      dome.video->get_isPrepared() &&
      value.opacity > 0.001f;

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
  if (_callbackController != nullptr) return _callbackController->get_songTime();
  if (!Alive(_audioController)) {
    _audioController =
        UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  }
  return Alive(_audioController) ? _audioController->get_songTime() : 0.0f;
}

float Runtime::TimeScale() {
  if (!Alive(_audioController)) {
    _audioController =
        UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  }
  return Alive(_audioController) ? Clamp(_audioController->get_timeScale(), 0.1f, 2.0f)
                                 : 1.0f;
}

void Runtime::UpdateVideo(DomeLayer& dome, float songTime, float realtime) {
  if (!Alive(dome.video) || dome.prepareFailed) return;

  try {
    if (!dome.video->get_isPrepared()) {
      if (dome.prepareStartedRealtime > 0.0f &&
          realtime - dome.prepareStartedRealtime > GetPrepareTimeoutSeconds()) {
        dome.prepareFailed = true;
        dome.pendingPlay = false;
        if (Alive(dome.renderer)) dome.renderer->set_enabled(false);
        PaperLogger.error(
            "Nexora decoder timeout on '{}' after {}s. Check adb logcat for AndroidVideoMedia.",
            dome.media, GetPrepareTimeoutSeconds());
      }
      return;
    }

    if (dome.pendingPlay) {
      if (dome.video->get_canSetTime()) {
        double const desired =
            dome.syncToSong
                ? std::max(0.0f,
                           songTime - dome.eventStartSongTime + dome.videoOffset)
                : std::max(0.0f, dome.videoOffset);

        dome.video->set_time(desired);
      }

      dome.video->Play();
      dome.pendingPlay = false;
      PaperLogger.info("Nexora started playback on dome '{}'", dome.id);
    }

    auto textureReference = dome.video->get_texture();
    auto* decoderTexture = textureReference.unsafePtr();
    auto* texture = dome.renderTexturePipeline && Alive(dome.videoTexture)
                        ? static_cast<UnityEngine::Texture*>(dome.videoTexture)
                        : decoderTexture;
    // frameReady is authoritative. Polling remains as a fallback for Android
    // video backends that advance VideoPlayer.frame but omit the event.
    bool const decodedFrame = dome.video->get_frame() >= 0;

    bool const hadTexture = dome.textureBound;
    dome.textureBound = dome.textureBound || (Alive(texture) && decodedFrame);
    if (dome.textureBound && Alive(dome.material) && s_propMainTex != 0) {
      dome.material->SetTexture(s_propMainTex, texture);
    }

    if (!hadTexture && dome.textureBound) {
      PaperLogger.info(
          "Nexora first decoded frame bound on dome '{}' pipeline={}",
          dome.id, dome.renderTexturePipeline ? "RenderTexture" : "APIOnly");
    }

    if (Alive(dome.renderer)) {
      dome.renderer->set_enabled(
          dome.textureBound &&
          dome.video->get_isPrepared() &&
          dome.visual.opacity > 0.001f);
    }

    if (!dome.video->get_isPlaying()) return;

    if (dome.video->get_canSetPlaybackSpeed()) {
      float const desiredSpeed = Clamp(dome.authoredPlaybackSpeed * TimeScale(), 0.1f, 4.0f);
      if (std::fabs(dome.video->get_playbackSpeed() - desiredSpeed) > 0.005f) {
        dome.video->set_playbackSpeed(desiredSpeed);
      }
    }

    if (!dome.syncToSong || !dome.video->get_canSetTime() ||
        realtime - dome.lastSyncRealtime < 0.5f) {
      return;
    }

    dome.lastSyncRealtime = realtime;
    double desired = std::max(0.0f, songTime - dome.eventStartSongTime + dome.videoOffset);
    double const length = dome.video->get_length();
    if (dome.looping && length > 0.01) desired = std::fmod(desired, length);
    double const drift = std::fabs(dome.video->get_time() - desired);
    if (drift > GetSyncToleranceSeconds()) {
      dome.video->set_time(desired);
      if (GetDebugLoggingEnabled()) {
        PaperLogger.info("Nexora resync dome '{}': drift={:.3f}s target={:.3f}s",
                         dome.id, drift, desired);
      }
    }
  } catch (std::exception const& ex) {
    PaperLogger.error("Nexora UpdateVideo exception on dome '{}': {}", dome.id, ex.what());
  } catch (...) {
    PaperLogger.error("Nexora UpdateVideo non-standard exception on dome '{}'", dome.id);
  }
}

void Runtime::OnVideoFrameReady(UnityEngine::Video::VideoPlayer* player,
                                std::int64_t frameIndex) {
  if (!Alive(player) || frameIndex < 0) return;

  for (auto& [_, dome] : _domes) {
    if (dome.video != player) continue;

    auto textureReference = player->get_texture();
    auto* decoderTexture = textureReference.unsafePtr();
    auto* texture = dome.renderTexturePipeline && Alive(dome.videoTexture)
                        ? static_cast<UnityEngine::Texture*>(dome.videoTexture)
                        : decoderTexture;
    if (!Alive(texture)) return;

    bool const firstFrame = !dome.textureBound;
    dome.textureBound = true;
    if (Alive(dome.material) && s_propMainTex != 0) {
      dome.material->SetTexture(s_propMainTex, texture);
    }
    if (Alive(dome.renderer)) {
      dome.renderer->set_enabled(dome.visual.opacity > 0.001f);
    }

    // One callback is enough for visibility. Turning the per-frame callback
    // back off avoids dispatching 60 managed events per second for a 4K60 map
    // video on Quest.
    player->set_sendFrameReadyEvents(false);
    if (firstFrame) {
      PaperLogger.info(
          "Nexora frameReady revealed dome '{}' at decoded frame {} pipeline={}",
          dome.id, frameIndex,
          dome.renderTexturePipeline ? "RenderTexture" : "APIOnly");
    }
    return;
  }
}

void Runtime::OnVideoPrepared(UnityEngine::Video::VideoPlayer* player) {
  if (!Alive(player)) return;
  for (auto& [_, dome] : _domes) {
    if (dome.video != player) continue;
    PaperLogger.info(
        "Nexora decoder prepared dome '{}' media='{}' size={}x{} length={:.3f}s",
        dome.id, dome.media, player->get_width(), player->get_height(),
        player->get_length());
    return;
  }
}

void Runtime::OnVideoError(UnityEngine::Video::VideoPlayer* player,
                           StringW message) {
  for (auto& [_, dome] : _domes) {
    if (dome.video != player) continue;
    dome.prepareFailed = true;
    dome.pendingPlay = false;
    if (Alive(dome.renderer)) dome.renderer->set_enabled(false);
    PaperLogger.error("Nexora decoder error on dome '{}' media='{}': {}", dome.id,
                      dome.media, std::string(message));
    return;
  }
  PaperLogger.error("Nexora decoder error after dome retirement: {}",
                    std::string(message));
}

void Runtime::UpdateDomes(float songTime) {
  auto cameraReference = UnityEngine::Camera::get_main();
  auto* camera = cameraReference.unsafePtr();
  UnityEngine::Vector3 cameraPosition = UnityEngine::Vector3::get_zero();
  if (Alive(camera)) {
    auto cameraTransform = camera->get_transform();
    if (Alive(cameraTransform.unsafePtr())) cameraPosition = cameraTransform->get_position();
  }
  float const realtime = UnityEngine::Time::get_realtimeSinceStartup();
  for (auto& [_, dome] : _domes) {
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
        auto base = dome.followPlayer ? cameraPosition : UnityEngine::Vector3::get_zero();
        transform->set_position(UnityEngine::Vector3(base.x + dome.offset.x,
                                                      base.y + dome.offset.y,
                                                      base.z + dome.offset.z));
      }
    }
    UpdateVideo(dome, songTime, realtime);
    ApplyDomeVisual(dome);
  }
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
      if (!Alive(dome.video)) continue;
      bool const wasPlaying = dome.video->get_isPlaying();
      dome.resumeAfterPause = dome.resumeAfterPause || wasPlaying;
      if (wasPlaying) dome.video->Pause();
    }
  } else {
    _lifecycle.Resume();
    for (auto& [_, dome] : _domes) {
      if (Alive(dome.video) && dome.resumeAfterPause && dome.video->get_isPrepared()) {
        dome.video->Play();
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
  try {
    if (Alive(layer.video)) {
      if (layer.frameReadyDelegate != nullptr) {
        layer.video->remove_frameReady(layer.frameReadyDelegate);
      }
      if (layer.prepareCompletedDelegate != nullptr) {
        layer.video->remove_prepareCompleted(layer.prepareCompletedDelegate);
      }
      if (layer.errorReceivedDelegate != nullptr) {
        layer.video->remove_errorReceived(layer.errorReceivedDelegate);
      }
      layer.video->set_sendFrameReadyEvents(false);
      layer.video->Stop();
      layer.video->set_targetTexture(nullptr);
    }
  } catch (...) {}
  if (!canTouchUnity) return;
  try {
    if (Alive(layer.material)) UnityEngine::Object::Destroy(layer.material);
    if (Alive(layer.videoTexture)) {
      layer.videoTexture->Release();
      UnityEngine::Object::Destroy(layer.videoTexture);
    }
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
  [[maybe_unused]] bool const retirementStarted = _lifecycle.BeginRetirement();
  if (_lifecycle.RenderDepth() != 0) {
    _pendingReset = true;
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
  _lastSongTime = -1.0f;
  _pendingReset = false;
  [[maybe_unused]] bool const retirementCompleted = _lifecycle.CompleteRetirement();
}

void Runtime::FinishPendingReset() { ResetSession(false); }

void Runtime::HandleScenesWillDismiss() { ResetSession(true); }

void Runtime::HandleGameplayRestart() { ResetSession(false); }

void Runtime::OnBehaviourDestroyed(RuntimeBehaviour* behaviour) {
  if (_behaviour == behaviour) _behaviour = nullptr;
}

}  // namespace Nexora
