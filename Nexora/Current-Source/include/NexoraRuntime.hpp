#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "NexoraLifecycle.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "UnityEngine/AssetBundle.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Video/VideoPlayer.hpp"
#include "custom-json-data/shared/CustomEventData.h"
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

namespace CustomJSONData {
class CustomBeatmapData;
}

namespace Nexora {

class RuntimeBehaviour;
inline constexpr std::string_view kCapability = "Nexora";
inline constexpr std::string_view kCreateDomeEvent = "Nexora.CreateDome";
inline constexpr std::string_view kLoadVideoEvent = "Nexora.LoadVideo";
inline constexpr std::string_view kPlayVideoEvent = "Nexora.PlayVideo";
inline constexpr std::string_view kPauseVideoEvent = "Nexora.PauseVideo";
inline constexpr std::string_view kStopVideoEvent = "Nexora.StopVideo";
inline constexpr std::string_view kSeekVideoEvent = "Nexora.SeekVideo";
inline constexpr std::string_view kSetPlaybackEvent = "Nexora.SetPlayback";
inline constexpr std::string_view kSetDomeEvent = "Nexora.SetDome";
inline constexpr std::string_view kAnimateDomeEvent = "Nexora.AnimateDome";
inline constexpr std::string_view kTransitionEvent = "Nexora.Transition";
inline constexpr std::string_view kPulseEvent = "Nexora.Pulse";
inline constexpr std::string_view kShockwaveEvent = "Nexora.Shockwave";
inline constexpr std::string_view kSetCameraEffectEvent = "Nexora.SetCameraEffect";
inline constexpr std::string_view kAnimateCameraEffectEvent = "Nexora.AnimateCameraEffect";
inline constexpr std::string_view kGlitchBurstEvent = "Nexora.GlitchBurst";
inline constexpr std::string_view kClearCameraEffectEvent = "Nexora.ClearCameraEffect";
inline constexpr std::string_view kDestroyDomeEvent = "Nexora.DestroyDome";
inline constexpr std::string_view kDestroyAllEvent = "Nexora.DestroyAll";

enum class Ease : std::uint8_t { Linear, InOut, OutCubic, InCubic, SmoothStep };

struct DomeVisual {
  float radius = 80.0f;
  float opacity = 1.0f;
  float brightness = 1.0f;
  float exposure = 0.0f;
  float saturation = 1.0f;
  float hueShift = 0.0f;
  float yaw = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float scaleZ = 1.0f;
  float deform = 0.0f;
  float deformFrequency = 3.0f;
  float deformSpeed = 1.0f;
  float ripple = 0.0f;
  float rippleFrequency = 8.0f;
  float rippleSpeed = 1.0f;
  float twist = 0.0f;
  float pinch = 0.0f;
  float pulse = 0.0f;
  float kaleidoscope = 0.0f;
  float pixelate = 0.0f;
  float chromatic = 0.0f;
  float scanline = 0.0f;
  float vignette = 0.0f;
  float fog = 0.0f;
  float projection = 0.0f;
  float flipX = 0.0f;
  float flipY = 0.0f;
  float swapEyes = 0.0f;
  UnityEngine::Color tint = UnityEngine::Color(1.0f, 1.0f, 1.0f, 1.0f);
};

struct CameraVisual {
  float amount = 0.0f;
  float fisheye = 0.0f;
  float chromatic = 0.0f;
  float glitch = 0.0f;
  float vignette = 0.0f;
  float scanline = 0.0f;
  float pixelate = 0.0f;
  float grayscale = 0.0f;
  float exposure = 0.0f;
  float hueShift = 0.0f;
  float split = 0.0f;
  float shake = 0.0f;
  float swirl = 0.0f;
  float kaleidoscope = 0.0f;
  UnityEngine::Color tint = UnityEngine::Color(1.0f, 1.0f, 1.0f, 1.0f);
};

template <typename T>
struct Animation {
  T start{};
  T target{};
  float startSongTime = 0.0f;
  float duration = 0.0f;
  Ease ease = Ease::Linear;
  bool active = false;
};

struct DomeLayer {
  std::string id;
  UnityEngine::GameObject* object = nullptr;
  UnityEngine::MeshFilter* filter = nullptr;
  UnityEngine::Mesh* mesh = nullptr;
  UnityEngine::Renderer* renderer = nullptr;
  UnityEngine::Material* material = nullptr;
  UnityEngine::Video::VideoPlayer* video = nullptr;
  UnityEngine::Video::VideoPlayer_FrameReadyEventHandler* frameReadyDelegate = nullptr;
  UnityEngine::Video::VideoPlayer_EventHandler* prepareCompletedDelegate = nullptr;
  UnityEngine::Video::VideoPlayer_ErrorEventHandler* errorReceivedDelegate = nullptr;
  UnityEngine::RenderTexture* videoTexture = nullptr;
  DomeVisual visual{};
  Animation<DomeVisual> animation{};
  UnityEngine::Vector3 offset = UnityEngine::Vector3::get_zero();
  bool followPlayer = true;
  bool pendingPlay = false;
  bool syncToSong = true;
  bool resumeAfterPause = false;
  bool prepareFailed = false;
  bool textureBound = false;
  bool customShader = false;
  bool renderTexturePipeline = false;
  bool looping = false;
  float eventStartSongTime = 0.0f;
  float videoOffset = 0.0f;
  float prepareStartedRealtime = 0.0f;
  float lastSyncRealtime = -1000.0f;
  float authoredPlaybackSpeed = 1.0f;
  std::string media;
};

class Runtime final {
public:
  static Runtime& Instance();

  void LateLoad();
  void Update();
  void OnVideoFrameReady(UnityEngine::Video::VideoPlayer* player, std::int64_t frameIndex);
  void OnVideoPrepared(UnityEngine::Video::VideoPlayer* player);
  void OnVideoError(UnityEngine::Video::VideoPlayer* player, StringW message);
  void SetPaused(bool paused);
  void SetApplicationPaused(bool paused);
  void SetFocused(bool focused);
  void SetSelectedMapRoot(std::string mapRoot, bool requiresNexora);
  void HandleScenesWillDismiss();
  void HandleGameplayRestart();
  void OnBehaviourDestroyed(RuntimeBehaviour* behaviour);
  bool HasQuestShaderAssets() const;

private:
  Runtime() = default;
  static void OnCustomEventStatic(GlobalNamespace::BeatmapCallbacksController* callbackController,
                                  CustomJSONData::CustomEventData* customEventData);
  void HandleCustomEvent(GlobalNamespace::BeatmapCallbacksController* callbackController,
                         CustomJSONData::CustomEventData* customEventData);
  CustomJSONData::CustomBeatmapData* GetCustomBeatmapData(
      GlobalNamespace::BeatmapCallbacksController* callbackController) const;
  bool PrepareBeatmap(GlobalNamespace::BeatmapCallbacksController* callbackController,
                      float triggerTime, std::string_view source);
  void TryPrepareSelectedBeatmapFromScene();
  void ReplayMissedEvents(float upToTime);
  bool IsNexoraEvent(std::string_view type) const;
  rapidjson::Value const* EventJson(CustomJSONData::CustomEventData* eventData) const;
  void BeginSession(GlobalNamespace::BeatmapCallbacksController* callbackController);
  void ResetSession(bool sceneTransition);
  void FinishPendingReset();
  void EnsureBehaviour();
  void LoadAssets();
  std::string QuestShaderAssetFailure() const;
  void InitPropertyIds();

  UnityEngine::Mesh* CreateProceduralDomeMesh(int rings, int segments, float radius);
  UnityEngine::Shader* FindUsableShader();

  DomeLayer* EnsureDome(std::string const& id);
  void DestroyDome(std::string const& id, bool canTouchUnity = true);
  void DestroyAllDomes(bool canTouchUnity = true);
  void LoadVideo(DomeLayer& dome, rapidjson::Value const& json, float eventTime);
  void PlayVideo(DomeLayer& dome, rapidjson::Value const& json, float eventTime);
  void PauseVideo(DomeLayer& dome);
  void StopVideo(DomeLayer& dome);
  void SeekVideo(DomeLayer& dome, rapidjson::Value const& json);
  void SetPlayback(DomeLayer& dome, rapidjson::Value const& json);
  void ApplyDomeJson(DomeVisual& visual, DomeLayer& dome, rapidjson::Value const& json);
  void ApplyDomeVisual(DomeLayer& dome);
  void AnimateDome(DomeLayer& dome, rapidjson::Value const& json, float eventTime);
  void UpdateDomes(float songTime);
  void UpdateVideo(DomeLayer& dome, float songTime, float realtime);

  void ApplyCameraJson(CameraVisual& visual, rapidjson::Value const& json);
  void SetCameraEffect(rapidjson::Value const& json, float eventTime, bool animated);
  void EnsureQuestSafeCameraEffects();
  void UpdateCameraAnimation(float songTime);
  void ApplyPauseState();

  float SongTime();
  float TimeScale();
  std::string DomeId(rapidjson::Value const& json) const;
  std::string ResolveMediaUrl(rapidjson::Value const& json) const;

  RuntimeBehaviour* _behaviour = nullptr;
  RuntimeLifecycle _lifecycle;
  GlobalNamespace::BeatmapCallbacksController* _callbackController = nullptr;
  CustomJSONData::CustomBeatmapData* _currentBeatmapData = nullptr;
  GlobalNamespace::AudioTimeSyncController* _audioController = nullptr;
  // These Unity assets outlive individual gameplay scenes. A raw IL2CPP pointer
  // is not a managed root and can become stale when Unity unloads unused assets
  // during a menu transition. Keep fixed GC handles and validate the native
  // Unity object before every use.
  SafePtrUnity<UnityEngine::AssetBundle> _assetBundle;
  SafePtrUnity<UnityEngine::Material> _domeTemplate;
  SafePtrUnity<UnityEngine::Shader> _domeShader;
  std::unordered_map<std::string, DomeLayer> _domes;
  CameraVisual _cameraVisual{};
  Animation<CameraVisual> _cameraAnimation{};
  std::uint64_t _sessionGeneration = 0;
  bool _paused = false;
  bool _applicationPaused = false;
  bool _focused = true;
  bool _pendingReset = false;
  bool _loggedMissingAssets = false;
  bool _loggedQuestSafeCameraEffects = false;
  bool _propertyIdsInitialized = false;
  bool _selectedMapRequiresNexora = false;
  bool _preparingBeatmap = false;
  int _lastUpdateErrorFrame = -10000;
  int _nextGameplayProbeFrame = -1;
  float _lastSongTime = -1.0f;
  std::string _selectedMapRoot;
  std::unordered_set<CustomJSONData::CustomEventData*> _processedNexoraEvents;
};

void LateLoad();

}  // namespace Nexora
