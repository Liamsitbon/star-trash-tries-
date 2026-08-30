#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "UnityEngine/AssetBundle.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Video/VideoPlayer.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

namespace CinemaQuest {

class RuntimeBehaviour;

struct ScreenPlacement {
  UnityEngine::Vector3 position = UnityEngine::Vector3(0.0f, 12.4f, 67.8f);
  UnityEngine::Vector3 rotation = UnityEngine::Vector3(-7.0f, 0.0f, 0.0f);
  UnityEngine::Vector3 scale = UnityEngine::Vector3::get_one();
};

struct VideoConfig {
  std::string sourceConfig;
  std::string videoPath;
  float offsetSeconds = 0.0f;
  float playbackSpeed = 1.0f;
  bool loop = false;
  float screenHeight = 25.0f;
  float curvatureDegrees = 0.0f;
  bool curveYAxis = false;
  int subsurfaces = 32;
  float brightness = 1.0f;
  float contrast = 1.0f;
  float saturation = 1.0f;
  float hueDegrees = 0.0f;
  float exposure = 1.0f;
  float gamma = 1.0f;
  float opacity = 1.0f;
  ScreenPlacement mainScreen{};
  std::vector<ScreenPlacement> additionalScreens;
};

struct ScreenInstance {
  UnityEngine::GameObject* object = nullptr;
  UnityEngine::MeshFilter* filter = nullptr;
  UnityEngine::Mesh* mesh = nullptr;
  UnityEngine::Renderer* renderer = nullptr;
  UnityEngine::Material* material = nullptr;
  ScreenPlacement placement{};
};

class Runtime final {
public:
  static Runtime& Instance();

  void LateLoad();
  void SetSelectedMapContext(std::string mapRoot, bool requiresCinema,
                             bool requiresNexora, bool requiresNoodleExtensions,
                             bool requiresVivify);
  void RefreshCapabilityRegistration(bool enabled);
  void BeginGameplay(GlobalNamespace::AudioTimeSyncController* audioController,
                     float startTimeOffset);
  void SetPaused(bool paused);
  void SetApplicationPaused(bool paused);
  void SetFocused(bool focused);
  void SetEnabled(bool enabled);
  void Update();
  void OnVideoFrameReady(UnityEngine::Video::VideoPlayer* player,
                         std::int64_t frameIndex);
  void OnVideoPrepared(UnityEngine::Video::VideoPlayer* player);
  void OnVideoError(UnityEngine::Video::VideoPlayer* player, StringW message);
  void OnBehaviourDestroyed(RuntimeBehaviour* behaviour);

private:
  Runtime() = default;

  void EnsureBehaviour();
  void LoadAssets();
  void LoadSelectedConfig();
  std::optional<VideoConfig> ParseConfig(std::string const& mapRoot) const;
  void CreatePlaybackGraph();
  void CreateScreen(ScreenPlacement const& placement);
  UnityEngine::Mesh* CreateScreenMesh(float height, float aspectRatio,
                                      float curvatureDegrees, bool curveYAxis,
                                      int subsurfaces) const;
  void RebuildScreenMeshes(float aspectRatio);
  void ApplyMaterialProperties(ScreenInstance& screen) const;
  void SetScreensVisible(bool visible);
  void ApplyPauseState();
  void StopSession();
  double DesiredVideoTime(float songTime) const;
  float DesiredPlaybackSpeed() const;

  RuntimeBehaviour* _behaviour = nullptr;
  SafePtrUnity<UnityEngine::AssetBundle> _assetBundle;
  SafePtrUnity<UnityEngine::Shader> _videoShader;
  GlobalNamespace::AudioTimeSyncController* _audioController = nullptr;
  UnityEngine::GameObject* _playbackObject = nullptr;
  UnityEngine::Video::VideoPlayer* _videoPlayer = nullptr;
  UnityEngine::Video::VideoPlayer_FrameReadyEventHandler* _frameReadyDelegate = nullptr;
  UnityEngine::Video::VideoPlayer_EventHandler* _prepareCompletedDelegate = nullptr;
  UnityEngine::Video::VideoPlayer_ErrorEventHandler* _errorReceivedDelegate = nullptr;
  UnityEngine::RenderTexture* _videoTexture = nullptr;
  std::vector<ScreenInstance> _screens;
  std::optional<VideoConfig> _selectedConfig;
  std::string _selectedMapRoot;
  bool _sessionActive = false;
  bool _paused = false;
  bool _applicationPaused = false;
  bool _focused = true;
  bool _firstFrameReady = false;
  bool _playIssued = false;
  bool _slowStartPending = false;
  bool _aspectApplied = false;
  bool _decoderFailed = false;
  bool _yieldToNexora = false;
  bool _selectedMapRequiresCinema = false;
  bool _selectedMapRequiresNexora = false;
  bool _selectedMapRequiresNoodleExtensions = false;
  bool _selectedMapRequiresVivify = false;
  float _prepareStartedRealtime = 0.0f;
  float _lastSyncRealtime = -1000.0f;
  int _lastErrorFrame = -10000;
};

void InstallHooks();

}  // namespace CinemaQuest
