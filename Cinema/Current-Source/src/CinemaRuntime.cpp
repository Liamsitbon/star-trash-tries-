#include "CinemaRuntime.hpp"

#include "QuestInterop.hpp"
#include "main.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "songcore/shared/Capabilities.hpp"
#include "songcore/shared/SongCore.hpp"

namespace CinemaQuest {
namespace {

constexpr std::string_view kCapability = "Cinema";
constexpr std::string_view kAssetPath =
    "/sdcard/ModData/com.beatgames.beatsaber/Mods/Cinema/Assets/cinemaassets.android";
constexpr std::string_view kAlternateAssetPath =
    "/storage/emulated/0/ModData/com.beatgames.beatsaber/Mods/Cinema/Assets/cinemaassets.android";
constexpr std::string_view kShaderAsset =
    "assets/cinema/shaders/cinemavideoscreen.shader";
constexpr float kPrepareTimeoutSeconds = 20.0f;
constexpr float kFirstFrameTimeoutSeconds = 8.0f;
constexpr int kNativeVideoWidth = 1920;
constexpr int kNativeVideoHeight = 1080;

int sMainTex = 0;
int sBrightness = 0;
int sContrast = 0;
int sSaturation = 0;
int sHue = 0;
int sExposure = 0;
int sGamma = 0;
int sOpacity = 0;

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

std::optional<float> ReadFloat(rapidjson::Value const& object,
                               char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsNumber()) {
    return std::nullopt;
  }
  return iterator->value.GetFloat();
}

std::optional<bool> ReadBool(rapidjson::Value const& object,
                             char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsBool()) {
    return std::nullopt;
  }
  return iterator->value.GetBool();
}

std::optional<std::string> ReadString(rapidjson::Value const& object,
                                      char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd() || !iterator->value.IsString()) {
    return std::nullopt;
  }
  return std::string(iterator->value.GetString(),
                     iterator->value.GetStringLength());
}

std::optional<UnityEngine::Vector3> ReadVector3(rapidjson::Value const& object,
                                                 char const* key) {
  auto iterator = object.FindMember(key);
  if (iterator == object.MemberEnd()) return std::nullopt;
  auto const& value = iterator->value;
  if (value.IsObject()) {
    auto x = ReadFloat(value, "x");
    auto y = ReadFloat(value, "y");
    auto z = ReadFloat(value, "z");
    if (x && y && z) return UnityEngine::Vector3(*x, *y, *z);
  }
  if (value.IsArray() && value.Size() >= 3 && value[0].IsNumber() &&
      value[1].IsNumber() && value[2].IsNumber()) {
    return UnityEngine::Vector3(value[0].GetFloat(), value[1].GetFloat(),
                                value[2].GetFloat());
  }
  return std::nullopt;
}

bool SupportedVideoExtension(std::filesystem::path const& path) {
  static std::unordered_set<std::string> const extensions = {
      ".mp4", ".m4v", ".mov", ".webm"};
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return extensions.contains(extension);
}

bool IsPathInside(std::filesystem::path const& root,
                  std::filesystem::path const& candidate) {
  auto rootIterator = root.begin();
  auto candidateIterator = candidate.begin();
  for (; rootIterator != root.end(); ++rootIterator, ++candidateIterator) {
    if (candidateIterator == candidate.end() ||
        *rootIterator != *candidateIterator) {
      return false;
    }
  }
  return true;
}

std::optional<std::filesystem::path> ResolveLocalVideo(
    std::filesystem::path const& root, std::string requested) {
  std::replace(requested.begin(), requested.end(), '\\', '/');
  std::filesystem::path relative(requested);

  // Old video.json files can contain a Windows absolute path. Never use that
  // path on Quest; recover only its filename and keep resolution inside the
  // selected map directory.
  if (requested.find(':') != std::string::npos || relative.is_absolute() ||
      relative.has_root_directory()) {
    relative = relative.filename();
  }
  for (auto const& component : relative) {
    if (component == "..") relative = relative.filename();
  }
  if (relative.empty()) return std::nullopt;

  std::vector<std::filesystem::path> candidates{relative};
  if (relative.extension().empty()) candidates.emplace_back(relative.string() + ".mp4");
  for (auto const& item : candidates) {
    std::error_code error;
    auto candidate = std::filesystem::weakly_canonical(root / item, error);
    if (error || !IsPathInside(root, candidate) ||
        !std::filesystem::is_regular_file(candidate, error) ||
        !SupportedVideoExtension(candidate)) {
      continue;
    }
    return candidate;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> FindMapVideo(
    std::filesystem::path const& root) {
  std::error_code error;
  std::vector<std::filesystem::path> videos;
  for (std::filesystem::directory_iterator iterator(root, error), end;
       !error && iterator != end; iterator.increment(error)) {
    if (!iterator->is_regular_file(error) ||
        !SupportedVideoExtension(iterator->path())) {
      continue;
    }
    videos.push_back(iterator->path());
  }
  if (videos.empty()) return std::nullopt;
  std::sort(videos.begin(), videos.end());
  return videos.front();
}

void InitializePropertyIds() {
  if (sMainTex != 0) return;
  sMainTex = UnityEngine::Shader::PropertyToID(u"_MainTex");
  sBrightness = UnityEngine::Shader::PropertyToID(u"_Brightness");
  sContrast = UnityEngine::Shader::PropertyToID(u"_Contrast");
  sSaturation = UnityEngine::Shader::PropertyToID(u"_Saturation");
  sHue = UnityEngine::Shader::PropertyToID(u"_Hue");
  sExposure = UnityEngine::Shader::PropertyToID(u"_Exposure");
  sGamma = UnityEngine::Shader::PropertyToID(u"_Gamma");
  sOpacity = UnityEngine::Shader::PropertyToID(u"_Opacity");
}

}  // namespace

Runtime& Runtime::Instance() {
  static Runtime runtime;
  return runtime;
}

void Runtime::LateLoad() {
  if (_initialized) {
    RefreshCapabilityRegistration(GetCinemaEnabled());
    return;
  }
  _initialized = true;
  RefreshCapabilityRegistration(GetCinemaEnabled());
  SongCore::API::LevelSelect::GetLevelWasSelectedEvent() +=
      [](SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
        std::string root;
        QuestModInterop::PeerSet required;
        if (event.isCustom && event.customBeatmapLevel != nullptr) {
          root = std::string(event.customBeatmapLevel->customLevelPath);
        }
        if (event.isCustom && event.customLevelDetails) {
          required = QuestModInterop::RequiredPeers(
              event.customLevelDetails->difficultyDetails.requirements);
        }
        auto const installed = QuestModInterop::InstalledPeers();
        PaperLogger.info(
            "Cinema interop: installed[C={} N={} NE={} V={}] required[C={} N={} NE={} V={}]",
            installed.cinema, installed.nexora, installed.noodleExtensions,
            installed.vivify, required.cinema, required.nexora,
            required.noodleExtensions, required.vivify);
        Runtime::Instance().SetSelectedMapContext(
            std::move(root), required.cinema, required.nexora,
            required.noodleExtensions, required.vivify);
      };
  PaperLogger.info(
      "Cinema Quest runtime armed without creating Unity objects: local map video only; no downloader, ffmpeg, URL playback or PC payloads");
}

void Runtime::LoadAssets() {
  if (Alive(_videoShader) && _videoShader->get_isSupported()) return;
  try {
    if (!Alive(_assetBundle)) {
      for (auto const path : {kAssetPath, kAlternateAssetPath}) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(std::string(path), error)) continue;
        auto bundleReference =
            UnityEngine::AssetBundle::LoadFromFile(StringW(std::string(path)));
        auto* bundle = bundleReference.unsafePtr();
        if (Alive(bundle)) {
          _assetBundle = bundle;
          PaperLogger.info("Cinema loaded Android shader bundle from {}", path);
          break;
        }
      }
    }
    if (Alive(_assetBundle)) {
      auto asset = _assetBundle->LoadAsset(StringW(std::string(kShaderAsset)));
      _videoShader =
          il2cpp_utils::try_cast<UnityEngine::Shader>(asset.unsafePtr()).value_or(nullptr);
    }
    if (!Alive(_videoShader) || !_videoShader->get_isSupported()) {
      for (auto const* name : {"Unlit/Texture", "BeatSaber/UnlitGlow"}) {
        auto shaderReference = UnityEngine::Shader::Find(StringW(name));
        auto* shader = shaderReference.unsafePtr();
        if (Alive(shader) && shader->get_isSupported()) {
          _videoShader = shader;
          PaperLogger.warn("Cinema is using fallback shader '{}'", name);
          break;
        }
      }
    }
    if (Alive(_videoShader)) {
      PaperLogger.info("Cinema video shader ready: '{}' supported={}",
                       _videoShader->get_name(), _videoShader->get_isSupported());
    } else {
      PaperLogger.error(
          "Cinema has no usable Quest shader. Reinstall the complete Cinema QMOD.");
    }
  } catch (std::exception const& exception) {
    PaperLogger.error("Cinema asset load failed safely: {}", exception.what());
  } catch (...) {
    PaperLogger.error("Cinema asset load failed with a non-standard exception");
  }
}

void Runtime::SetSelectedMapContext(std::string mapRoot, bool requiresCinema,
                                    bool requiresNexora,
                                    bool requiresNoodleExtensions,
                                    bool requiresVivify) {
  if (!mapRoot.empty()) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(mapRoot, error);
    if (error || !std::filesystem::is_directory(normalized, error)) {
      mapRoot.clear();
    } else {
      mapRoot = normalized.string();
    }
  }
  bool const nextYieldToNexora = requiresNexora && !requiresCinema;
  _selectedMapRoot = std::move(mapRoot);
  _selectedMapRequiresCinema = requiresCinema;
  _selectedMapRequiresNexora = requiresNexora;
  _selectedMapRequiresNoodleExtensions = requiresNoodleExtensions;
  _selectedMapRequiresVivify = requiresVivify;
  _yieldToNexora = nextYieldToNexora;
  if (_yieldToNexora) {
    _selectedConfig.reset();
    PaperLogger.info(
        "Cinema yielded map video ownership to required Nexora; an explicit Cinema+Nexora requirement keeps both active");
    return;
  }
  if (GetCinemaEnabled()) {
    LoadSelectedConfig();
  } else {
    _selectedConfig.reset();
  }
}

void Runtime::LoadSelectedConfig() {
  _selectedConfig.reset();
  if (_selectedMapRoot.empty() || _yieldToNexora) return;
  _selectedConfig = ParseConfig(_selectedMapRoot);
  if (_selectedConfig) {
    PaperLogger.info("Cinema selected config='{}' video='{}' offset={:.3f}s speed={:.3f}",
                     _selectedConfig->sourceConfig,
                     std::filesystem::path(_selectedConfig->videoPath).filename().string(),
                     _selectedConfig->offsetSeconds,
                     _selectedConfig->playbackSpeed);
  }
}

std::optional<VideoConfig> Runtime::ParseConfig(
    std::string const& mapRoot) const {
  std::filesystem::path const root(mapRoot);
  std::filesystem::path configPath;
  for (auto const* name : {"cinema-video.json", "video.json"}) {
    std::error_code error;
    auto candidate = root / name;
    if (std::filesystem::is_regular_file(candidate, error)) {
      configPath = candidate;
      break;
    }
  }
  if (configPath.empty()) return std::nullopt;

  std::ifstream stream(configPath, std::ios::binary | std::ios::ate);
  if (!stream.is_open()) return std::nullopt;
  auto const size = stream.tellg();
  if (size <= 0 || size > 2 * 1024 * 1024) {
    PaperLogger.warn("Cinema rejected oversized or empty config {}", configPath.string());
    return std::nullopt;
  }
  std::string json(static_cast<std::size_t>(size), '\0');
  stream.seekg(0);
  stream.read(json.data(), static_cast<std::streamsize>(size));

  rapidjson::Document document;
  document.Parse(json.data(), json.size());
  if (document.HasParseError() || !document.IsObject()) {
    PaperLogger.warn("Cinema could not parse {} at byte {}", configPath.string(),
                     document.GetErrorOffset());
    return std::nullopt;
  }

  rapidjson::Value const* value = &document;
  auto videos = document.FindMember("videos");
  if (videos != document.MemberEnd() && videos->value.IsArray()) {
    int active = 0;
    auto activeMember = document.FindMember("activeVideo");
    if (activeMember != document.MemberEnd() && activeMember->value.IsInt()) {
      active = activeMember->value.GetInt();
    }
    if (active < 0 || active >= static_cast<int>(videos->value.Size()) ||
        !videos->value[active].IsObject()) {
      PaperLogger.warn("Cinema legacy video.json has an invalid activeVideo index");
      return std::nullopt;
    }
    value = &videos->value[active];
  }

  VideoConfig config;
  config.sourceConfig = configPath.filename().string();
  std::string requested = ReadString(*value, "videoFile")
                              .value_or(ReadString(*value, "videoPath").value_or(""));
  auto video = requested.empty() ? FindMapVideo(root)
                                 : ResolveLocalVideo(root, requested);
  if (!video) {
    PaperLogger.warn(
        "Cinema config '{}' has no readable local map video; network URLs are intentionally unsupported on Quest",
        config.sourceConfig);
    return std::nullopt;
  }
  config.videoPath = video->string();
  config.offsetSeconds =
      Clamp(ReadFloat(*value, "offset").value_or(0.0f) / 1000.0f,
            -3600.0f, 3600.0f);
  float speed = ReadFloat(*value, "playbackSpeed").value_or(1.0f);
  if (speed > 10.0f) speed /= 100.0f;
  config.playbackSpeed = Clamp(speed, 0.1f, 4.0f);
  config.loop = ReadBool(*value, "loop").value_or(false);
  config.opacity = Clamp(ReadFloat(*value, "opacity").value_or(1.0f), 0.0f, 1.0f);
  config.bloom = Clamp(ReadFloat(*value, "bloom").value_or(0.0f), 0.0f, 2.0f);
  if (auto endVideoAt = ReadFloat(*value, "endVideoAt");
      endVideoAt.has_value() && *endVideoAt >= 0.0f) {
    config.endVideoAt = Clamp(*endVideoAt, 0.0f, 24.0f * 60.0f * 60.0f);
  }
  config.screenHeight =
      Clamp(ReadFloat(*value, "screenHeight").value_or(25.0f), 0.5f, 100.0f);
  config.curvatureDegrees =
      Clamp(ReadFloat(*value, "screenCurvature").value_or(0.0f), -180.0f, 180.0f);
  config.curveYAxis = ReadBool(*value, "curveYAxis").value_or(false);
  config.subsurfaces = std::clamp(
      static_cast<int>(ReadFloat(*value, "screenSubsurfaces").value_or(32.0f)),
      1, 64);
  config.mainScreen.position =
      ReadVector3(*value, "screenPosition").value_or(config.mainScreen.position);
  config.mainScreen.rotation =
      ReadVector3(*value, "screenRotation").value_or(config.mainScreen.rotation);

  auto colorCorrection = value->FindMember("colorCorrection");
  if (colorCorrection != value->MemberEnd() && colorCorrection->value.IsObject()) {
    auto const& color = colorCorrection->value;
    config.brightness = Clamp(ReadFloat(color, "brightness").value_or(1.0f), 0.0f, 4.0f);
    config.contrast = Clamp(ReadFloat(color, "contrast").value_or(1.0f), 0.0f, 5.0f);
    config.saturation = Clamp(ReadFloat(color, "saturation").value_or(1.0f), 0.0f, 5.0f);
    config.hueDegrees = Clamp(ReadFloat(color, "hue").value_or(0.0f), -360.0f, 360.0f);
    config.exposure = Clamp(ReadFloat(color, "exposure").value_or(1.0f), 0.0f, 5.0f);
    config.gamma = Clamp(ReadFloat(color, "gamma").value_or(1.0f), 0.1f, 5.0f);
  }

  auto additional = value->FindMember("additionalScreens");
  if (additional != value->MemberEnd() && additional->value.IsArray()) {
    for (auto const& item : additional->value.GetArray()) {
      if (!item.IsObject() || config.additionalScreens.size() >= 8) continue;
      ScreenPlacement placement = config.mainScreen;
      placement.position = ReadVector3(item, "position").value_or(placement.position);
      placement.rotation = ReadVector3(item, "rotation").value_or(placement.rotation);
      placement.scale = ReadVector3(item, "scale").value_or(placement.scale);
      config.additionalScreens.push_back(placement);
    }
  }
  return config;
}

void Runtime::BeginGameplay(
    GlobalNamespace::AudioTimeSyncController* audioController,
    float startTimeOffset) {
  // A previous scene owns any abandoned objects. Never probe stale Unity
  // pointers while a new gameplay scene is being constructed.
  StopSession(false);
  if (!GetCinemaEnabled() || !_selectedConfig || !Alive(audioController)) return;
  LoadAssets();
  if (!Alive(_videoShader)) {
    PaperLogger.error("Cinema skipped gameplay because its Quest shader is unavailable");
    return;
  }

  _audioController = audioController;
  _sessionActive = true;
  _paused = false;
  _decoderFailed = false;
  try {
    CreatePlaybackGraph();
    PaperLogger.info(
        "Cinema gameplay started practiceOffset={:.3f}s songTime={:.3f}s screens={}",
        startTimeOffset, audioController->get_songTime(), _screens.size());
  } catch (std::exception const& exception) {
    PaperLogger.error("Cinema could not create playback graph: {}", exception.what());
    StopSession(true);
  } catch (...) {
    PaperLogger.error("Cinema could not create playback graph (non-standard exception)");
    StopSession(true);
  }
}

void Runtime::CreatePlaybackGraph() {
  InitializePropertyIds();
  _playbackObject = UnityEngine::GameObject::New_ctor(u"CinemaQuestPlayback");
  if (!Alive(_playbackObject)) throw std::runtime_error("playback GameObject creation failed");

  if (!_nativeVideo) {
    _nativeVideo = QuestNativeVideo::Player::Create(
        kNativeVideoWidth, kNativeVideoHeight, "Cinema");
  }
  if (!_nativeVideo) throw std::runtime_error("Android native video backend unavailable");

  CreateScreen(_selectedConfig->mainScreen);
  for (auto const& placement : _selectedConfig->additionalScreens) {
    CreateScreen(placement);
  }
  SetScreensVisible(false);

  _prepareStartedRealtime = UnityEngine::Time::get_realtimeSinceStartup();
  _nativeVideo->Open(_selectedConfig->videoPath, _selectedConfig->loop);
  PaperLogger.info("Cinema preparing local video '{}' via Android MediaPlayer surface {}x{}",
                   std::filesystem::path(_selectedConfig->videoPath).filename().string(),
                   kNativeVideoWidth, kNativeVideoHeight);
}

void Runtime::CreateScreen(ScreenPlacement const& placement) {
  auto* object = UnityEngine::GameObject::New_ctor(
      StringW("CinemaQuestScreen_" + std::to_string(_screens.size())));
  if (!Alive(object)) throw std::runtime_error("screen GameObject creation failed");
  auto* filter = object->AddComponent<UnityEngine::MeshFilter*>();
  auto* renderer = object->AddComponent<UnityEngine::MeshRenderer*>();
  if (!Alive(filter) || !Alive(renderer)) {
    UnityEngine::Object::Destroy(object);
    throw std::runtime_error("screen MeshFilter/MeshRenderer creation failed");
  }
  auto* mesh = CreateScreenMesh(_selectedConfig->screenHeight, 16.0f / 9.0f,
                                _selectedConfig->curvatureDegrees,
                                _selectedConfig->curveYAxis,
                                _selectedConfig->subsurfaces);
  auto* material = UnityEngine::Material::New_ctor(_videoShader.ptr());
  if (!Alive(mesh) || !Alive(material)) {
    if (Alive(mesh)) UnityEngine::Object::Destroy(mesh);
    UnityEngine::Object::Destroy(object);
    throw std::runtime_error("screen mesh/material creation failed");
  }
  filter->set_sharedMesh(mesh);
  renderer->set_sharedMaterial(material);
  renderer->set_receiveShadows(false);
  renderer->set_enabled(false);

  auto transformReference = object->get_transform();
  auto* transform = transformReference.unsafePtr();
  if (Alive(transform)) {
    if (Alive(_playbackObject)) {
      auto parentReference = _playbackObject->get_transform();
      if (Alive(parentReference.unsafePtr())) {
        transform->SetParent(parentReference.unsafePtr(), false);
      }
    }
    transform->set_position(placement.position);
    transform->set_rotation(UnityEngine::Quaternion::Euler(placement.rotation));
    transform->set_localScale(placement.scale);
  }

  ScreenInstance screen;
  screen.object = object;
  screen.filter = filter;
  screen.mesh = mesh;
  screen.renderer = renderer;
  screen.material = material;
  screen.placement = placement;
  ApplyMaterialProperties(screen);
  _screens.push_back(screen);
}

UnityEngine::Mesh* Runtime::CreateScreenMesh(
    float height, float aspectRatio, float curvatureDegrees, bool curveYAxis,
    int subsurfaces) const {
  float const width = height * Clamp(aspectRatio, 0.2f, 5.0f);
  int const curvedSegments = std::clamp(subsurfaces, 1, 64);
  int const xSegments = curveYAxis ? 1 : curvedSegments;
  int const ySegments = curveYAxis ? curvedSegments : 1;
  int const vertexCount = (xSegments + 1) * (ySegments + 1);
  int const indexCount = xSegments * ySegments * 6;
  auto vertices = ArrayW<UnityEngine::Vector3>(vertexCount);
  auto normals = ArrayW<UnityEngine::Vector3>(vertexCount);
  auto uvs = ArrayW<UnityEngine::Vector2>(vertexCount);
  auto triangles = ArrayW<int32_t>(indexCount);

  float const radians = std::fabs(curvatureDegrees) *
                        static_cast<float>(M_PI) / 180.0f;
  float const direction = curvatureDegrees < 0.0f ? -1.0f : 1.0f;
  int vertexIndex = 0;
  for (int y = 0; y <= ySegments; ++y) {
    float const v = static_cast<float>(y) / static_cast<float>(ySegments);
    for (int x = 0; x <= xSegments; ++x) {
      float const u = static_cast<float>(x) / static_cast<float>(xSegments);
      float px = (u - 0.5f) * width;
      float py = (v - 0.5f) * height;
      float pz = 0.0f;
      if (radians > 0.001f) {
        if (curveYAxis) {
          float const radius = height / radians;
          float const angle = (v - 0.5f) * radians;
          py = std::sin(angle) * radius;
          pz = (std::cos(angle) - 1.0f) * radius * direction;
        } else {
          float const radius = width / radians;
          float const angle = (u - 0.5f) * radians;
          px = std::sin(angle) * radius;
          pz = (std::cos(angle) - 1.0f) * radius * direction;
        }
      }
      vertices[vertexIndex] = UnityEngine::Vector3(px, py, pz);
      normals[vertexIndex] = UnityEngine::Vector3(0.0f, 0.0f, -1.0f);
      uvs[vertexIndex] = UnityEngine::Vector2(u, v);
      ++vertexIndex;
    }
  }

  int triangleIndex = 0;
  for (int y = 0; y < ySegments; ++y) {
    for (int x = 0; x < xSegments; ++x) {
      int const current = y * (xSegments + 1) + x;
      int const next = current + xSegments + 1;
      triangles[triangleIndex++] = current;
      triangles[triangleIndex++] = next;
      triangles[triangleIndex++] = current + 1;
      triangles[triangleIndex++] = current + 1;
      triangles[triangleIndex++] = next;
      triangles[triangleIndex++] = next + 1;
    }
  }

  auto* mesh = UnityEngine::Mesh::New_ctor();
  mesh->set_name(u"CinemaQuestCurvedScreenMesh");
  mesh->set_vertices(vertices);
  mesh->set_normals(normals);
  mesh->set_uv(uvs);
  mesh->set_triangles(triangles);
  mesh->RecalculateBounds();
  return mesh;
}

void Runtime::RebuildScreenMeshes(float aspectRatio) {
  for (auto& screen : _screens) {
    auto* replacement = CreateScreenMesh(
        _selectedConfig->screenHeight, aspectRatio,
        _selectedConfig->curvatureDegrees, _selectedConfig->curveYAxis,
        _selectedConfig->subsurfaces);
    if (!Alive(replacement)) continue;
    auto* previous = screen.mesh;
    screen.mesh = replacement;
    if (Alive(screen.filter)) screen.filter->set_sharedMesh(replacement);
    if (Alive(previous)) UnityEngine::Object::Destroy(previous);
  }
}

void Runtime::ApplyMaterialProperties(ScreenInstance& screen) const {
  if (!Alive(screen.material) || !_selectedConfig) return;
  screen.material->set_renderQueue(1998);
  if (_nativeVideo && _nativeVideo->Texture() != nullptr) {
    screen.material->SetTexture(sMainTex, _nativeVideo->Texture());
  }
  // Quest's verified bundle has no PC BloomPrePass. Preserve the authored
  // bloom intent as a bounded luminance gain without allocating a camera pass.
  screen.material->SetFloat(
      sBrightness,
      Clamp(_selectedConfig->brightness * (1.0f + _selectedConfig->bloom * 0.25f),
            0.0f, 4.0f));
  screen.material->SetFloat(sContrast, _selectedConfig->contrast);
  screen.material->SetFloat(sSaturation, _selectedConfig->saturation);
  screen.material->SetFloat(sHue, _selectedConfig->hueDegrees / 360.0f);
  screen.material->SetFloat(sExposure, _selectedConfig->exposure);
  screen.material->SetFloat(sGamma, _selectedConfig->gamma);
  screen.material->SetFloat(sOpacity, _selectedConfig->opacity);
}

void Runtime::ApplyPlaybackFade(float value) const {
  if (!_selectedConfig) return;
  float const opacity = _selectedConfig->opacity * Clamp(value, 0.0f, 1.0f);
  for (auto const& screen : _screens) {
    if (Alive(screen.material)) screen.material->SetFloat(sOpacity, opacity);
  }
}

void Runtime::SetScreensVisible(bool visible) {
  bool const shouldShow = visible && _firstFrameReady && GetCinemaEnabled() &&
                          _sessionActive && !_decoderFailed;
  for (auto& screen : _screens) {
    if (Alive(screen.renderer)) screen.renderer->set_enabled(shouldShow);
  }
}

double Runtime::DesiredVideoTime(float songTime) const {
  if (!_selectedConfig) return 0.0;
  double desired = static_cast<double>(songTime) *
                       static_cast<double>(_selectedConfig->playbackSpeed) +
                   static_cast<double>(_selectedConfig->offsetSeconds);
  if (_selectedConfig->loop && _nativeVideo) {
    double const length = _nativeVideo->DurationSeconds();
    if (length > 0.01 && desired >= 0.0) desired = std::fmod(desired, length);
  }
  return desired;
}

float Runtime::DesiredPlaybackSpeed() const {
  float timeScale = Alive(_audioController) ? _audioController->get_timeScale() : 1.0f;
  float authored = _selectedConfig ? _selectedConfig->playbackSpeed : 1.0f;
  return Clamp(timeScale * authored, 0.1f, 4.0f);
}

void Runtime::Update() {
  if (!_sessionActive) return;
  try {
    if (!_selectedConfig || !GetCinemaEnabled() || !_nativeVideo ||
        _decoderFailed) {
      return;
    }
    _nativeVideo->Tick();
    float const realtime = UnityEngine::Time::get_realtimeSinceStartup();
    if (_nativeVideo->Failed()) {
      _decoderFailed = true;
      SetScreensVisible(false);
      PaperLogger.error("Cinema native decoder failed for '{}': {}",
                        std::filesystem::path(_selectedConfig->videoPath)
                            .filename()
                            .string(),
                        _nativeVideo->Error());
      _nativeVideo->Stop();
      return;
    }
    if (!_nativeVideo->IsPrepared()) {
      if (realtime - _prepareStartedRealtime > kPrepareTimeoutSeconds) {
        _decoderFailed = true;
        SetScreensVisible(false);
        PaperLogger.error(
            "Cinema Android MediaPlayer timeout after {}s for '{}'",
            kPrepareTimeoutSeconds,
            std::filesystem::path(_selectedConfig->videoPath).filename().string());
        _nativeVideo->Stop();
      }
      return;
    }

    if (!_aspectApplied) {
      int const width = _nativeVideo->VideoWidth();
      int const height = _nativeVideo->VideoHeight();
      float aspect = 16.0f / 9.0f;
      if (width > 0 && height > 0) {
        aspect = Clamp(static_cast<float>(width) / static_cast<float>(height),
                       0.2f, 5.0f);
      }
      RebuildScreenMeshes(aspect);
      _aspectApplied = true;
      PaperLogger.info("Cinema decoder prepared source={}x{} aspect={:.3f}",
                       width, height, aspect);
    }

    bool const suspended = _paused;
    if (suspended || !Alive(_audioController)) return;

    float const songTime = _audioController->get_songTime();
    double const desired = DesiredVideoTime(songTime);
    if (!std::isfinite(songTime) || !std::isfinite(desired)) {
      SetScreensVisible(false);
      PaperLogger.warn("Cinema ignored a non-finite song clock value");
      return;
    }
    double const length = _nativeVideo->DurationSeconds();
    if (desired < 0.0) {
      if (_nativeVideo->IsPlaying()) _nativeVideo->Pause();
      _playIssued = false;
      SetScreensVisible(false);
      return;
    }
    double const authoredEnd = _selectedConfig->endVideoAt.has_value()
                                   ? static_cast<double>(*_selectedConfig->endVideoAt)
                                   : (!_selectedConfig->loop && length > 0.01
                                          ? length
                                          : -1.0);
    if (authoredEnd > 0.0 && desired >= authoredEnd) {
      if (_nativeVideo->IsPlaying()) _nativeVideo->Pause();
      SetScreensVisible(false);
      return;
    }
    ApplyPlaybackFade(authoredEnd > 0.0
                          ? static_cast<float>(std::min(1.0, authoredEnd - desired))
                          : 1.0f);

    if (!_playIssued) {
      float const speed = DesiredPlaybackSpeed();
      bool const needsInitialSeek = desired > 0.05;
      if (needsInitialSeek) _nativeVideo->Seek(desired);
      _nativeVideo->Play();
      _nativeVideo->SetPlaybackSpeed(speed);
      _playIssued = true;
      _playStartedRealtime = realtime;
      _lastSyncRealtime = realtime;
      PaperLogger.info(
          "Cinema native playback issued target={:.3f}s speed={:.3f} initialSeek={}",
          desired, speed, needsInitialSeek);
      return;
    }

    if (!_firstFrameReady && _playStartedRealtime > 0.0f &&
        realtime - _playStartedRealtime > kFirstFrameTimeoutSeconds) {
      _decoderFailed = true;
      SetScreensVisible(false);
      PaperLogger.error(
          "Cinema native decoder produced no first frame within {}s for '{}'",
          kFirstFrameTimeoutSeconds,
          std::filesystem::path(_selectedConfig->videoPath).filename().string());
      _nativeVideo->Stop();
      return;
    }

    if (!_firstFrameReady && _nativeVideo->HasFrame() &&
        _nativeVideo->Texture() != nullptr) {
      _firstFrameReady = true;
      for (auto& screen : _screens) {
        if (Alive(screen.material)) {
          screen.material->SetTexture(sMainTex, _nativeVideo->Texture());
        }
      }
      SetScreensVisible(true);
      PaperLogger.info(
          "Cinema first native frame revealed {} screen(s), serial={}",
          _screens.size(), _nativeVideo->FrameSerial());
    }

    // A transient Android decoder pause must not leave a map permanently
    // frozen. Authored pause/end paths return above, so this recovery is only
    // reached while normal playback is still requested.
    if (!_nativeVideo->IsPlaying()) _nativeVideo->Play();

    float const speed = DesiredPlaybackSpeed();
    if (std::fabs(_nativeVideo->PlaybackSpeed() - speed) > 0.005f) {
      _nativeVideo->SetPlaybackSpeed(speed);
    }
    if (realtime - _lastSyncRealtime < 0.5f) {
      return;
    }
    _lastSyncRealtime = realtime;
    double const actual = _nativeVideo->TimeSeconds();
    if (std::isfinite(actual) && std::fabs(actual - desired) > 0.12) {
      _nativeVideo->Seek(desired);
      PaperLogger.info("Cinema resync drift={:.3f}s target={:.3f}s song={:.3f}s",
                       std::fabs(actual - desired), desired, songTime);
    }
  } catch (std::exception const& exception) {
    int const frame = UnityEngine::Time::get_frameCount();
    if (frame - _lastErrorFrame >= 90) {
      _lastErrorFrame = frame;
      PaperLogger.error("Cinema update skipped a failing frame: {}", exception.what());
    }
  } catch (...) {
    int const frame = UnityEngine::Time::get_frameCount();
    if (frame - _lastErrorFrame >= 90) {
      _lastErrorFrame = frame;
      PaperLogger.error("Cinema update skipped a non-standard exception");
    }
  }
}

void Runtime::SetPaused(bool paused) {
  _paused = paused;
  ApplyPauseState();
}

void Runtime::ApplyPauseState() {
  if (!_sessionActive || !_nativeVideo) return;
  bool const suspended = _paused;
  if (suspended) {
    if (_nativeVideo->IsPlaying()) _nativeVideo->Pause();
    PaperLogger.info("Cinema playback suspended songTime={:.3f}s",
                     Alive(_audioController) ? _audioController->get_songTime() : -1.0f);
    return;
  }
  _lastSyncRealtime = -1000.0f;
  if (_playIssued && _nativeVideo->IsPrepared()) {
    _nativeVideo->Play();
    if (!_firstFrameReady) {
      _playStartedRealtime = UnityEngine::Time::get_realtimeSinceStartup();
    }
  }
  PaperLogger.info("Cinema playback resumed; immediate song-time resync scheduled");
}

void Runtime::SetEnabled(bool enabled) {
  RefreshCapabilityRegistration(enabled);
  if (!enabled) {
    StopSession(true);
  } else {
    LoadSelectedConfig();
  }
}

void Runtime::RefreshCapabilityRegistration(bool enabled) {
  bool const registered =
      SongCore::API::Capabilities::IsCapabilityRegistered(kCapability);
  if (enabled && !registered) {
    SongCore::API::Capabilities::RegisterCapability(kCapability);
    PaperLogger.info("Cinema capability registered because the runtime is enabled");
  } else if (!enabled && registered) {
    SongCore::API::Capabilities::UnregisterCapability(kCapability);
    SongCore::API::PlayButton::EnablePlayButton(std::string(kCapability));
    PaperLogger.info("Cinema capability unregistered because the runtime is disabled");
  }
}

void Runtime::StopSession(bool canTouchUnity) {
  if (_nativeVideo) _nativeVideo->Stop();
  if (canTouchUnity) try {
    for (auto& screen : _screens) {
      if (Alive(screen.material)) UnityEngine::Object::Destroy(screen.material);
      if (Alive(screen.mesh)) UnityEngine::Object::Destroy(screen.mesh);
      if (Alive(screen.object)) UnityEngine::Object::Destroy(screen.object);
    }
    if (Alive(_playbackObject)) UnityEngine::Object::Destroy(_playbackObject);
  } catch (...) {
    PaperLogger.warn(
        "Cinema ignored a Unity exception while retiring already-unloaded gameplay objects");
  }
  _screens.clear();

  _audioController = nullptr;
  _playbackObject = nullptr;
  _sessionActive = false;
  _firstFrameReady = false;
  _playIssued = false;
  _aspectApplied = false;
  _decoderFailed = false;
  _prepareStartedRealtime = 0.0f;
  _playStartedRealtime = 0.0f;
  _lastSyncRealtime = -1000.0f;
}

void Runtime::RetireGameplay(
    GlobalNamespace::AudioTimeSyncController* audioController,
    bool canTouchUnity) {
  if (_audioController != audioController) return;
  StopSession(canTouchUnity);
}

}  // namespace CinemaQuest
