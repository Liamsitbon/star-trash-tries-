#include "CinemaRuntime.hpp"

#include "CinemaComponents.hpp"
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
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Video/VideoAudioOutputMode.hpp"
#include "UnityEngine/Video/VideoRenderMode.hpp"
#include "UnityEngine/Video/VideoSource.hpp"
#include "custom-types/shared/delegate.hpp"
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
constexpr int kRenderTextureWidth = 1920;
constexpr int kRenderTextureHeight = 1080;

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
  if (GetCinemaEnabled()) {
    EnsureBehaviour();
    LoadAssets();
  } else {
    PaperLogger.info(
        "Cinema is disabled: no runtime GameObject, AssetBundle, RenderTexture or VideoPlayer was created");
  }
  SongCore::API::Capabilities::RegisterCapability(std::string(kCapability));
  SongCore::API::LevelSelect::GetLevelWasSelectedEvent() +=
      [](SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
        std::string root;
        if (event.isCustom && event.customBeatmapLevel != nullptr) {
          root = std::string(event.customBeatmapLevel->customLevelPath);
        }
        Runtime::Instance().SetSelectedMapRoot(std::move(root));
      };
  PaperLogger.info(
      "Cinema Quest runtime ready: local map video only; no downloader, ffmpeg, URL playback or PC payloads");
}

void Runtime::EnsureBehaviour() {
  if (Alive(_behaviour)) return;
  try {
    auto* object = UnityEngine::GameObject::New_ctor(u"CinemaQuestRuntime");
    if (!Alive(object)) return;
    UnityEngine::Object::DontDestroyOnLoad(object);
    _behaviour = object->AddComponent<RuntimeBehaviour*>();
  } catch (...) {
    PaperLogger.warn("Cinema runtime behaviour could not be created yet; gameplay will retry");
  }
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

void Runtime::SetSelectedMapRoot(std::string mapRoot) {
  if (!mapRoot.empty()) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(mapRoot, error);
    if (error || !std::filesystem::is_directory(normalized, error)) {
      mapRoot.clear();
    } else {
      mapRoot = normalized.string();
    }
  }
  _selectedMapRoot = std::move(mapRoot);
  if (GetCinemaEnabled()) {
    LoadSelectedConfig();
  } else {
    _selectedConfig.reset();
  }
}

void Runtime::LoadSelectedConfig() {
  _selectedConfig.reset();
  if (_selectedMapRoot.empty()) return;
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
  StopSession();
  if (!GetCinemaEnabled() || !_selectedConfig || !Alive(audioController)) return;
  EnsureBehaviour();
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
    StopSession();
  } catch (...) {
    PaperLogger.error("Cinema could not create playback graph (non-standard exception)");
    StopSession();
  }
}

void Runtime::CreatePlaybackGraph() {
  InitializePropertyIds();
  _playbackObject = UnityEngine::GameObject::New_ctor(u"CinemaQuestPlayback");
  if (!Alive(_playbackObject)) throw std::runtime_error("playback GameObject creation failed");

  _videoPlayer =
      _playbackObject->AddComponent<UnityEngine::Video::VideoPlayer*>();
  if (!Alive(_videoPlayer)) throw std::runtime_error("Unity VideoPlayer is unavailable");

  _videoTexture = UnityEngine::RenderTexture::New_ctor(
      kRenderTextureWidth, kRenderTextureHeight, 0,
      UnityEngine::RenderTextureFormat::ARGB32);
  if (!Alive(_videoTexture)) throw std::runtime_error("video RenderTexture allocation failed");
  _videoTexture->set_name(u"CinemaQuestVideoTexture");
  _videoTexture->set_filterMode(UnityEngine::FilterMode::Bilinear);
  _videoTexture->set_wrapMode(UnityEngine::TextureWrapMode::Mirror);
  if (!_videoTexture->Create()) throw std::runtime_error("video RenderTexture creation failed");

  _videoPlayer->set_source(UnityEngine::Video::VideoSource::Url);
  _videoPlayer->set_renderMode(UnityEngine::Video::VideoRenderMode::RenderTexture);
  _videoPlayer->set_targetTexture(_videoTexture);
  _videoPlayer->set_audioOutputMode(UnityEngine::Video::VideoAudioOutputMode::None);
  _videoPlayer->set_playOnAwake(false);
  _videoPlayer->set_waitForFirstFrame(true);
  _videoPlayer->set_isLooping(_selectedConfig->loop);
  if (_videoPlayer->get_canSetSkipOnDrop()) _videoPlayer->set_skipOnDrop(true);

  std::function<void(UnityEngine::Video::VideoPlayer*, std::int64_t)> frameReady =
      [](UnityEngine::Video::VideoPlayer* player, std::int64_t frameIndex) {
        Runtime::Instance().OnVideoFrameReady(player, frameIndex);
      };
  _frameReadyDelegate =
      custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_FrameReadyEventHandler*>(
          frameReady);
  _videoPlayer->add_frameReady(_frameReadyDelegate);
  _videoPlayer->set_sendFrameReadyEvents(true);

  std::function<void(UnityEngine::Video::VideoPlayer*)> prepareCompleted =
      [](UnityEngine::Video::VideoPlayer* player) {
        Runtime::Instance().OnVideoPrepared(player);
      };
  _prepareCompletedDelegate =
      custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_EventHandler*>(
          prepareCompleted);
  _videoPlayer->add_prepareCompleted(_prepareCompletedDelegate);

  std::function<void(UnityEngine::Video::VideoPlayer*, StringW)> errorReceived =
      [](UnityEngine::Video::VideoPlayer* player, StringW message) {
        Runtime::Instance().OnVideoError(player, message);
      };
  _errorReceivedDelegate =
      custom_types::MakeDelegate<UnityEngine::Video::VideoPlayer_ErrorEventHandler*>(
          errorReceived);
  _videoPlayer->add_errorReceived(_errorReceivedDelegate);

  CreateScreen(_selectedConfig->mainScreen);
  for (auto const& placement : _selectedConfig->additionalScreens) {
    CreateScreen(placement);
  }
  SetScreensVisible(false);

  _videoPlayer->set_url(StringW(_selectedConfig->videoPath));
  _prepareStartedRealtime = UnityEngine::Time::get_realtimeSinceStartup();
  _videoPlayer->Prepare();
  PaperLogger.info("Cinema preparing local video '{}' via {}x{} RenderTexture",
                   std::filesystem::path(_selectedConfig->videoPath).filename().string(),
                   kRenderTextureWidth, kRenderTextureHeight);
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
  screen.material->SetTexture(sMainTex, _videoTexture);
  screen.material->SetFloat(sBrightness, _selectedConfig->brightness);
  screen.material->SetFloat(sContrast, _selectedConfig->contrast);
  screen.material->SetFloat(sSaturation, _selectedConfig->saturation);
  screen.material->SetFloat(sHue, _selectedConfig->hueDegrees / 360.0f);
  screen.material->SetFloat(sExposure, _selectedConfig->exposure);
  screen.material->SetFloat(sGamma, _selectedConfig->gamma);
  screen.material->SetFloat(sOpacity, _selectedConfig->opacity);
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
  if (_selectedConfig->loop && Alive(_videoPlayer)) {
    double const length = _videoPlayer->get_length();
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
    auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
    if (scene.get_name() == u"MainMenu") {
      PaperLogger.info(
          "Cinema reached MainMenu; retiring the previous gameplay session on a stable frame");
      StopSession();
      return;
    }
    if (!_selectedConfig || !GetCinemaEnabled() || !Alive(_videoPlayer) ||
        _decoderFailed) {
      return;
    }
    float const realtime = UnityEngine::Time::get_realtimeSinceStartup();
    if (!_videoPlayer->get_isPrepared()) {
      if (realtime - _prepareStartedRealtime > kPrepareTimeoutSeconds) {
        _decoderFailed = true;
        SetScreensVisible(false);
        PaperLogger.error(
            "Cinema decoder timeout after {}s for '{}'; inspect AndroidVideoMedia in logcat",
            kPrepareTimeoutSeconds,
            std::filesystem::path(_selectedConfig->videoPath).filename().string());
      }
      return;
    }

    if (!_aspectApplied) {
      std::uint32_t const width = _videoPlayer->get_width();
      std::uint32_t const height = _videoPlayer->get_height();
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

    bool const suspended = _paused || _applicationPaused || !_focused;
    if (suspended || !Alive(_audioController)) return;

    float const songTime = _audioController->get_songTime();
    double const desired = DesiredVideoTime(songTime);
    double const length = _videoPlayer->get_length();
    if (desired < 0.0) {
      if (_videoPlayer->get_isPlaying()) _videoPlayer->Pause();
      if (_videoPlayer->get_canSetTime()) _videoPlayer->set_time(0.0);
      _playIssued = false;
      SetScreensVisible(false);
      return;
    }
    if (!_selectedConfig->loop && length > 0.01 && desired >= length) {
      if (_videoPlayer->get_isPlaying()) _videoPlayer->Pause();
      SetScreensVisible(false);
      return;
    }

    if (!_playIssued) {
      if (_videoPlayer->get_canSetTime()) _videoPlayer->set_time(desired);
      float const speed = DesiredPlaybackSpeed();
      _slowStartPending = speed < 0.999f && desired > 0.02;
      if (_videoPlayer->get_canSetPlaybackSpeed()) {
        _videoPlayer->set_playbackSpeed(_slowStartPending ? 1.0f : speed);
      }
      _videoPlayer->set_sendFrameReadyEvents(true);
      _videoPlayer->Play();
      _playIssued = true;
      _lastSyncRealtime = realtime;
      PaperLogger.info("Cinema playback issued target={:.3f}s speed={:.3f} stagedSlowStart={}",
                       desired, speed, _slowStartPending);
      return;
    }

    if (!_videoPlayer->get_isPlaying()) {
      _videoPlayer->Play();
    }
    if (_videoPlayer->get_canSetPlaybackSpeed() && !_slowStartPending) {
      float const speed = DesiredPlaybackSpeed();
      if (std::fabs(_videoPlayer->get_playbackSpeed() - speed) > 0.005f) {
        _videoPlayer->set_playbackSpeed(speed);
      }
    }
    if (!_videoPlayer->get_canSetTime() ||
        realtime - _lastSyncRealtime < 0.5f) {
      return;
    }
    _lastSyncRealtime = realtime;
    double const actual = _videoPlayer->get_time();
    if (std::isfinite(actual) && std::fabs(actual - desired) > 0.12) {
      _videoPlayer->set_time(desired);
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

void Runtime::OnVideoFrameReady(UnityEngine::Video::VideoPlayer* player,
                                std::int64_t frameIndex) {
  if (player != _videoPlayer || frameIndex < 0 || !_sessionActive) return;
  bool const first = !_firstFrameReady;
  _firstFrameReady = true;
  if (_slowStartPending && player->get_canSetPlaybackSpeed()) {
    player->set_playbackSpeed(DesiredPlaybackSpeed());
    _slowStartPending = false;
  }
  player->set_sendFrameReadyEvents(false);
  SetScreensVisible(true);
  if (first) {
    PaperLogger.info("Cinema frameReady revealed {} screen(s) at frame {}",
                     _screens.size(), frameIndex);
  }
}

void Runtime::OnVideoPrepared(UnityEngine::Video::VideoPlayer* player) {
  if (player != _videoPlayer || !_sessionActive || !Alive(player)) return;
  PaperLogger.info("Cinema decoder prepared media='{}' size={}x{} length={:.3f}s",
                   _selectedConfig
                       ? std::filesystem::path(_selectedConfig->videoPath)
                             .filename()
                             .string()
                       : std::string("<retired>"),
                   player->get_width(), player->get_height(),
                   player->get_length());
}

void Runtime::OnVideoError(UnityEngine::Video::VideoPlayer* player,
                           StringW message) {
  if (player != _videoPlayer || !_sessionActive) {
    PaperLogger.error("Cinema decoder error after session retirement: {}",
                      std::string(message));
    return;
  }
  _decoderFailed = true;
  _playIssued = false;
  SetScreensVisible(false);
  PaperLogger.error("Cinema decoder rejected media='{}': {}",
                    _selectedConfig
                        ? std::filesystem::path(_selectedConfig->videoPath)
                              .filename()
                              .string()
                        : std::string("<unknown>"),
                    std::string(message));
}

void Runtime::SetPaused(bool paused) {
  _paused = paused;
  ApplyPauseState();
}

void Runtime::SetApplicationPaused(bool paused) {
  _applicationPaused = paused;
  ApplyPauseState();
}

void Runtime::SetFocused(bool focused) {
  _focused = focused;
  ApplyPauseState();
}

void Runtime::ApplyPauseState() {
  if (!_sessionActive || !Alive(_videoPlayer)) return;
  bool const suspended = _paused || _applicationPaused || !_focused;
  if (suspended) {
    if (_videoPlayer->get_isPlaying()) _videoPlayer->Pause();
    PaperLogger.info("Cinema playback suspended songTime={:.3f}s",
                     Alive(_audioController) ? _audioController->get_songTime() : -1.0f);
    return;
  }
  _lastSyncRealtime = -1000.0f;
  if (_playIssued && _videoPlayer->get_isPrepared()) _videoPlayer->Play();
  PaperLogger.info("Cinema playback resumed; immediate song-time resync scheduled");
}

void Runtime::SetEnabled(bool enabled) {
  if (!enabled) {
    StopSession();
  } else {
    EnsureBehaviour();
    LoadAssets();
    LoadSelectedConfig();
  }
}

void Runtime::StopSession() {
  if (Alive(_videoPlayer)) {
    try {
      if (_frameReadyDelegate != nullptr) {
        _videoPlayer->remove_frameReady(_frameReadyDelegate);
      }
      if (_prepareCompletedDelegate != nullptr) {
        _videoPlayer->remove_prepareCompleted(_prepareCompletedDelegate);
      }
      if (_errorReceivedDelegate != nullptr) {
        _videoPlayer->remove_errorReceived(_errorReceivedDelegate);
      }
      _videoPlayer->set_sendFrameReadyEvents(false);
      _videoPlayer->Stop();
      _videoPlayer->set_targetTexture(nullptr);
    } catch (...) {
      PaperLogger.warn("Cinema caught an exception while stopping VideoPlayer");
    }
  }
  try {
    for (auto& screen : _screens) {
      if (Alive(screen.material)) UnityEngine::Object::Destroy(screen.material);
      if (Alive(screen.mesh)) UnityEngine::Object::Destroy(screen.mesh);
      if (Alive(screen.object)) UnityEngine::Object::Destroy(screen.object);
    }
    if (Alive(_videoTexture)) {
      _videoTexture->Release();
      UnityEngine::Object::Destroy(_videoTexture);
    }
    if (Alive(_playbackObject)) UnityEngine::Object::Destroy(_playbackObject);
  } catch (...) {
    PaperLogger.warn(
        "Cinema ignored a Unity exception while retiring already-unloaded gameplay objects");
  }
  _screens.clear();

  _audioController = nullptr;
  _playbackObject = nullptr;
  _videoPlayer = nullptr;
  _frameReadyDelegate = nullptr;
  _prepareCompletedDelegate = nullptr;
  _errorReceivedDelegate = nullptr;
  _videoTexture = nullptr;
  _sessionActive = false;
  _firstFrameReady = false;
  _playIssued = false;
  _slowStartPending = false;
  _aspectApplied = false;
  _decoderFailed = false;
  _prepareStartedRealtime = 0.0f;
  _lastSyncRealtime = -1000.0f;
}

void Runtime::OnBehaviourDestroyed(RuntimeBehaviour* behaviour) {
  if (_behaviour == behaviour) _behaviour = nullptr;
}

}  // namespace CinemaQuest
