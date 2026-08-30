#include "VivifyRuntimeInternal.hpp"
#include "VivifyComponents.hpp"
#include "UnityEngine/ParticleSystem.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"

namespace Vivify {

namespace {
bool IsMainMenuSceneActive() {
  auto scene = UnityEngine::SceneManagement::SceneManager::GetActiveScene();
  return scene.get_name() == "MainMenu";
}

UnityEngine::AnimationState* DefaultLegacyAnimationState(
    UnityEngine::Animation* animation) {
  if (!IsManagedAlive(animation)) return nullptr;
  auto clip = animation->get_clip();
  auto* clipPtr = clip.unsafePtr();
  if (!IsManagedAlive(clipPtr)) return nullptr;
  return animation->get_Item(clipPtr->get_name());
}

void SetLegacyAnimationSpeed(UnityEngine::Animation* animation, float speed) {
  auto* state = DefaultLegacyAnimationState(animation);
  if (state != nullptr && UnityEngine::TrackedReference::op_Implicit_bool(state)) {
    state->set_speed(speed);
  }
}
}

Runtime& Runtime::Instance() {
  static Runtime runtime;
  return runtime;
}

bool Runtime::IsAlive(UnityEngine::Object* object) const {
  return IsManagedAlive(object);
}

void Runtime::LateLoad() {
  auto cjdModInfo = CustomJSONData::modInfo.to_c();
  auto tracksModInfo = CModInfo{.id = "Tracks"};
  modloader_require_mod(&cjdModInfo, CMatchType::MatchType_IdOnly);
  modloader_require_mod(&tracksModInfo, CMatchType::MatchType_IdOnly);
  EnsureBehaviour();
  if (!SongCore::API::Capabilities::IsCapabilityRegistered(kCapability)) {
    SongCore::API::Capabilities::RegisterCapability(kCapability);
  }
  CustomJSONData::CustomEventCallbacks::AddCustomEventCallback(&Runtime::OnCustomEventStatic);
  SongCore::API::LevelSelect::GetLevelWasSelectedEvent() += [](SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
    Runtime::Instance().HandleLevelSelected(event);
  };
}

void Runtime::EnsureBehaviour() {
  if (_behaviour != nullptr) {
    return;
  }
  auto* gameObject = UnityEngine::GameObject::New_ctor(u"VivifyRuntime");
  UnityEngine::Object::DontDestroyOnLoad(gameObject);
  _behaviour = gameObject->AddComponent<RuntimeBehaviour*>();
  RefreshCameraComponents(false);
}

void Runtime::OnBehaviourDestroyed(RuntimeBehaviour* behaviour) {
  if (_behaviour == behaviour) _behaviour = nullptr;
}

void Runtime::OnCustomEventStatic(GlobalNamespace::BeatmapCallbacksController* callbackController,
                                  CustomJSONData::CustomEventData* customEventData) {
  Runtime::Instance().HandleCustomEvent(callbackController, customEventData);
}

void Runtime::Update() {
  try {
    if (_isResetting) return;

    // Beatmap callbacks can outlive the gameplay scene by several frames.
    // Leaving authored materials and CameraApplier active during that window
    // attached them to MenuMainCamera and eventually passed a destroyed
    // Material into CommandBuffer.Blit.  The transition reset is deliberately
    // pointer-free, so it is safe even after Unity has begun scene teardown.
    if (_currentBeatmapData != nullptr && IsMainMenuSceneActive()) {
      PaperLogger.info("Vivify detected MainMenu with live beatmap state; applying transition-safe reset");
      ResetRuntime(ResetMode::LateSceneTransition);
      return;
    }

    if (_currentBeatmapData == nullptr) {
      // A seek can intentionally tear down live state. Rebuild from the active
      // gameplay callback, but do not probe every menu frame while Unity is
      // still constructing or destroying that callback.
      TryPrepareSelectedBeatmapFromScene();
      if (_currentBeatmapData == nullptr) return;
    }
    if (!_lifecycle.IsActive()) return;

    // Scene transitions can invalidate the cached controller before the
    // beatmap callbacks disappear. Reacquire it instead of permanently
    // disabling Vivify's frame updates for the rest of the level.
    if (!IsAlive(_audioTimeSyncController)) {
      _audioTimeSyncController =
          UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
    }
    if (!IsAlive(_audioTimeSyncController)) return;

    DetectSongRestart();
    // DetectSongRestart can call ResetRuntime(), which clears all beatmap
    // state. Never continue into the update systems after that reset.
    if (_isResetting || _currentBeatmapData == nullptr) return;

    // A malformed map component should not prevent unrelated systems (most
    // importantly Blit expiry) from running. Keep every subsystem isolated
    // and identify the failing one in the log.
    auto runUpdateStep = [this](char const* name, auto&& step) {
      try {
        step();
      } catch (std::exception const& ex) {
        LogThrottledUpdateError(std::string(name) + ": " + ex.what());
      } catch (...) {
        LogThrottledUpdateError(std::string(name) + ": non-std exception");
      }
    };

    runUpdateStep("SuppressAlwaysVisibleQuads", [this]() {
      int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
      if (frame < _nextAlwaysVisibleQuadScanFrame) return;
      // AlwaysVisibleQuad objects are sometimes recreated by the HUD after
      // beatmap preparation.  A periodic, bounded scan catches those late
      // instances without putting FindObjectsOfType on every render frame.
      _nextAlwaysVisibleQuadScanFrame = frame + 30;
      SuppressActiveAlwaysVisibleQuads();
    });
    runUpdateStep("UpdateStartupPostProcessingEvents",
                  [this]() { UpdateStartupPostProcessingEvents(); });
    runUpdateStep("UpdatePrefabPrewarm", [this]() { UpdatePrefabPrewarm(); });
    runUpdateStep("UpdateMaterialAnimations", [this]() { UpdateMaterialAnimations(); });
    runUpdateStep("UpdateGlobalAnimations", [this]() { UpdateGlobalAnimations(); });
    runUpdateStep("UpdateAnimatorAnimations", [this]() { UpdateAnimatorAnimations(); });
    runUpdateStep("UpdateRenderSettingAnimations",
                  [this]() { UpdateRenderSettingAnimations(); });
    runUpdateStep("UpdateBlitEffects", [this]() { UpdateBlitEffects(); });
    runUpdateStep("UpdateSyncedObjects", [this]() { UpdateSyncedObjects(); });
    runUpdateStep("UpdatePrefabAnimationSpeed",
                  [this]() { UpdatePrefabAnimationSpeed(); });

    runUpdateStep("DeferredSaberVisuals", [this]() {
      int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
      if (_deferredSaberVisualsUntilFrame < 0 ||
          frame < _deferredSaberVisualsUntilFrame) {
        return;
      }
      if (_currentBeatmapData != nullptr && !_isResetting) {
        VIVIFY_DEBUG("Vivify applying deferred/retry saber assignment");
        ApplySaberVisualsToActive();
      }
      _deferredSaberVisualsUntilFrame =
          frame < _saberVisualRetryUntilFrame ? frame + 15 : -1;
    });
    runUpdateStep("UpdateSaberReplacementColors",
                  [this]() { UpdateSaberReplacementColors(); });

    // Refresh last so a Blit dispatched by the startup queue is visible for
    // its intended frame, and so expired effects disable the applier again.
    runUpdateStep("RefreshCameraComponents",
                  [this]() { RefreshCameraComponents(true); });
    runUpdateStep("LogQuestPerformanceHeartbeat",
                  [this]() { LogQuestPerformanceHeartbeat(); });
  } catch (std::exception const& ex) {
    LogThrottledUpdateError(std::string("Runtime::Update: ") + ex.what());
  } catch (...) {
    LogThrottledUpdateError("Runtime::Update: non-std exception");
  }
}

void Runtime::TryPrepareSelectedBeatmapFromScene() {
  if (!_selectedMapHasVivifyRequirement || _selectedLevelPath.empty() ||
      _mainBundle == nullptr || !UnityEngine::Object::op_Implicit_bool(_mainBundle) ||
      _beatmapCallbacksController == nullptr) return;

  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  if (frame < _nextBeatmapPrepareProbeFrame) return;
  _nextBeatmapPrepareProbeFrame = frame + 15;

  auto* audioController =
      UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  if (!IsAlive(audioController)) return;

  auto* customBeatmapData = GetCustomBeatmapData(_beatmapCallbacksController);
  if (customBeatmapData == nullptr) return;
  float const triggerTime = std::max(0.0f, audioController->get_songTime());
  VIVIFY_DEBUG("Vivify proactive gameplay prepare: customEvents={} songTime={}",
               customBeatmapData->customEventDatas.size(), triggerTime);
  PrepareBeatmap(customBeatmapData, triggerTime, _beatmapCallbacksController);
}

void Runtime::LogThrottledUpdateError(std::string_view what) {

  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  if (frame - _lastUpdateErrorFrame < 60) return;
  _lastUpdateErrorFrame = frame;
  PaperLogger.error("Vivify Update threw (frame {}, songTime {}): {} — repeats suppressed ~1s",
                    frame, _songTimeCache, std::string(what));
}

float Runtime::CurrentSongTime() {

  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  if (frame == _songTimeCacheFrame) return _songTimeCache;
  _songTimeCacheFrame = frame;
  // BeatmapCallbacksController is the authoritative clock used to fire the
  // same callbacks that drive notes and cubes. Prefer it over a globally found
  // AudioTimeSyncController so Vivify cannot bind to a stale controller during
  // practice/catch-up or a gameplay-scene transition.
  if (_beatmapCallbacksController != nullptr) {
    _songTimeCache = _beatmapCallbacksController->get_songTime();
    return _songTimeCache;
  }
  if (!IsAlive(_audioTimeSyncController)) {
    _audioTimeSyncController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  }
  _songTimeCache = IsAlive(_audioTimeSyncController) ? _audioTimeSyncController->get_songTime() : 0.0f;
  return _songTimeCache;
}

void Runtime::DetectSongRestart() {
  if (_currentBeatmapData == nullptr || _isResetting) return;
  float songTime = CurrentSongTime();
  // Only reset if there's a significant backward jump (>0.25 second) to avoid
  // false positives at song start (0:00) or small timing adjustments
  if (_lastSongTime >= 0.0f && songTime + 0.25f < _lastSongTime) {
    int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
    bool const restartingFromPause =
        _pauseMenuActive && songTime <= 0.25f && _lastSongTime > 1.0f;
    if (restartingFromPause) {
      // The RestartButtonPressed hook normally retires the session before the
      // scene is dismissed. This remains as a live-scene fallback for practice
      // controllers that seek without invoking the pause-menu method.
      VIVIFY_DEBUG("Vivify detected pause-menu restart; retiring the active session");
      ResetRuntime(ResetMode::LiveScene);
      _nextBeatmapPrepareProbeFrame = frame + 90;
    } else {
      ResetRuntime();
    }
    return;
  }
  _lastSongTime = songTime;
}

float Runtime::CurrentBpm() const {
  if (TracksStatic::bpmController) {
    return TracksStatic::bpmController->get_currentBpm();
  }
  return 0.0f;
}

float Runtime::DurationBeatsToSeconds(float durationBeats) const {
  float bpm = CurrentBpm();
  if (bpm <= 0.0f) {
    return 0.0f;
  }
  return (60.0f * durationBeats) / bpm;
}

void Runtime::RegisterSyncedObject(UnityEngine::GameObject* root,
                                   float eventStartSongTime) {
  if (!IsAlive(root)) return;
  UnregisterSyncedObject(root);

  float const currentSongTime = CurrentSongTime();
  float const initialElapsed =
      PrefabInitialElapsed(currentSongTime, eventStartSongTime);
  SyncedObject synced;
  synced.root = root;
  // Videos remain tied to the absolute event time; only Animator.Update uses
  // the elapsed time inside the prefab.
  synced.startTime = eventStartSongTime;

  auto videoPlayers = root->GetComponentsInChildren<UnityEngine::Video::VideoPlayer*>(true);
  for (int i = 0; i < videoPlayers.size(); i++) {
    auto* vp = videoPlayers[i];
    if (!IsAlive(vp) || !vp->get_playOnAwake()) continue;

    // A number of Android Vivify ports (including RSIH) serialize an embedded
    // VideoPlayer in MaterialOverride mode but lose its target renderer during
    // the bundle conversion. Unity still prepares and advances the clip, so
    // the TV/Display stays on its static material with no obvious runtime
    // error. Desktop Vivify receives a valid target from the original prefab;
    // recover the equivalent same-GameObject renderer on Quest.
    if (vp->get_renderMode().value__ ==
        UnityEngine::Video::VideoRenderMode::MaterialOverride.value__) {
      auto targetReference = vp->get_targetMaterialRenderer();
      auto* target = targetReference.unsafePtr();
      if (!IsAlive(target)) {
        target = vp->GetComponent<UnityEngine::Renderer*>();
        if (!IsAlive(target)) {
          target = vp->GetComponentInChildren<UnityEngine::Renderer*>(true);
        }
        if (IsAlive(target)) {
          vp->set_targetMaterialRenderer(target);
          if (ToStdString(vp->get_targetMaterialProperty()).empty()) {
            vp->set_targetMaterialProperty(StringW("_MainTex"));
          }
          PaperLogger.info(
              "Vivify repaired embedded VideoPlayer target: object='{}' renderer='{}' property='{}'",
              ToStdString(vp->get_gameObject()->get_name()),
              ToStdString(target->get_gameObject()->get_name()),
              ToStdString(vp->get_targetMaterialProperty()));
        } else {
          PaperLogger.warn(
              "Vivify embedded VideoPlayer has no MaterialOverride renderer: object='{}'",
              ToStdString(vp->get_gameObject()->get_name()));
        }
      }
    }
    vp->set_skipOnDrop(false);
    if (!vp->get_isPrepared()) vp->Prepare();
    synced.videoPlayers.emplace_back(vp);
  }

  auto animators = root->GetComponentsInChildren<UnityEngine::Animator*>(true);
  for (int i = 0; i < animators.size(); i++) {
    if (!IsAlive(animators[i])) continue;
    animators[i]->set_updateMode(UnityEngine::AnimatorUpdateMode::Normal);
    if (initialElapsed > 0.0f) animators[i]->Update(initialElapsed);
    synced.animators.emplace_back(animators[i]);
  }
  auto legacyAnimations = root->GetComponentsInChildren<UnityEngine::Animation*>(true);
  for (int i = 0; i < legacyAnimations.size(); i++) {
    auto* animation = legacyAnimations[i];
    if (!IsAlive(animation) || !animation->get_playAutomatically()) continue;
    auto* state = DefaultLegacyAnimationState(animation);
    if (state == nullptr || !UnityEngine::TrackedReference::op_Implicit_bool(state)) continue;

    // Unity's legacy Animation clock is independent from AudioTimeSyncController.
    // Dynasty's lyrics and environment prefabs use this component rather than
    // Animator, so sample the authored clip at the actual practice/song time.
    if (initialElapsed > 0.0f) {
      state->set_time(initialElapsed);
      animation->Sample();
    }
    float initialSpeed = _pauseMenuActive ? 0.0f :
        (IsManagedAlive(_audioTimeSyncController)
             ? _audioTimeSyncController->get_timeScale()
             : 0.0f);
    state->set_speed(std::isfinite(initialSpeed) && initialSpeed > 0.0f
                         ? initialSpeed
                         : 0.0f);
    synced.legacyAnimations.emplace_back(animation);
  }
  auto particleSystems = root->GetComponentsInChildren<UnityEngine::ParticleSystem*>(true);
  for (int i = 0; i < particleSystems.size(); i++) {
    if (IsAlive(particleSystems[i])) synced.particleSystems.emplace_back(particleSystems[i]);
  }

  if (!synced.videoPlayers.empty() || !synced.animators.empty() ||
      !synced.legacyAnimations.empty() || !synced.particleSystems.empty()) {
    VIVIFY_DEBUG("Vivify sync: registered '{}' with {} video / {} animator / {} legacy animation / {} particle component(s), eventStart={} songTime={} initialElapsed={}",
                 IsAlive(root) ? ToStdString(root->get_name()) : std::string("?"),
                 synced.videoPlayers.size(), synced.animators.size(),
                 synced.legacyAnimations.size(), synced.particleSystems.size(),
                 eventStartSongTime, currentSongTime, initialElapsed);
    _syncedObjects.emplace_back(std::move(synced));
    // A newly registered animator/particle system must receive the current
    // practice speed even if that speed did not change this frame.
    _lastAppliedSyncSpeed = std::numeric_limits<float>::quiet_NaN();
  }
}

void Runtime::UpdatePrefabAnimationSpeed() {
  if (_syncedObjects.empty()) {
    _lastSyncSongTime = _beatmapCallbacksController != nullptr ||
                                IsManagedAlive(_audioTimeSyncController)
                            ? CurrentSongTime()
                            : -1.0f;
    _lastAppliedSyncSpeed = std::numeric_limits<float>::quiet_NaN();
    return;
  }

  bool const controllerUnavailable = !IsManagedAlive(_audioTimeSyncController);
  float const songTime = controllerUnavailable ? -1.0f : CurrentSongTime();
  float const timeScale = controllerUnavailable ? 0.0f : _audioTimeSyncController->get_timeScale();
  float const speed = StableSyncRate(_pauseMenuActive || controllerUnavailable,
                                     _lastSyncSongTime, songTime, timeScale);
  if (!controllerUnavailable) _lastSyncSongTime = songTime;

  if (!MeaningfulRateChange(_lastAppliedSyncSpeed, speed)) return;
  _lastAppliedSyncSpeed = speed;

  auto drive = [speed](std::vector<UnityEngine::Animator*> const& animators,
                       std::vector<UnityEngine::Animation*> const& legacyAnimations,
                       std::vector<UnityEngine::ParticleSystem*> const& particleSystems) {
    for (auto* animator : animators) {
      if (IsManagedAlive(animator)) animator->set_speed(speed);
    }
    for (auto* animation : legacyAnimations) {
      SetLegacyAnimationSpeed(animation, speed);
    }
    for (auto* particleSystem : particleSystems) {
      if (!IsManagedAlive(particleSystem)) continue;
      auto main = particleSystem->get_main();
      main.set_simulationSpeed(speed);
    }
  };
  for (auto& synced : _syncedObjects) {
    drive(synced.animators, synced.legacyAnimations, synced.particleSystems);
  }
}

void Runtime::LogQuestPerformanceHeartbeat() {
  if (!GetVivifyDebugLogging()) return;
  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  if (frame - _lastQuestPerformanceFrame < 900) return;
  _lastQuestPerformanceFrame = frame;
  PaperLogger.info(
      "Vivify Quest perf window: frame={} midCacheHits={} midRebuilds={} imageBlits={} imageCommandBufferCreates={} noteRefreshEvents={} noteRefreshCandidates={} prefabLookupRebuilds={} synced={} livePrefabs={} replacements[note={} saber={} debris={}] declaredTextures={} secondaryCameras={}",
      frame, _midCommandBufferCacheHits, _midCommandBufferRebuilds,
      _imageBlitExecutions, _imageCommandBufferCreates, _noteRefreshEvents,
      _noteRefreshCandidates, _assignedPrefabLookupRebuilds, _syncedObjects.size(),
      _livePrefabs.size(), _noteReplacements.size(), _saberReplacements.size(),
      _debrisReplacements.size(), _declaredTextures.size(), _secondaryCameras.size());
  _midCommandBufferCacheHits = 0;
  _midCommandBufferRebuilds = 0;
  _imageBlitExecutions = 0;
  _imageCommandBufferCreates = 0;
  _noteRefreshEvents = 0;
  _noteRefreshCandidates = 0;
  _assignedPrefabLookupRebuilds = 0;
}

void Runtime::UnregisterSyncedObject(UnityEngine::GameObject* root) {
  _syncedObjects.erase(std::remove_if(_syncedObjects.begin(), _syncedObjects.end(),
                                      [root](SyncedObject const& synced) { return synced.root == root; }),
                       _syncedObjects.end());
}

void Runtime::UpdateSyncedObjects() {
  if (_syncedObjects.empty()) return;
  _syncedObjects.erase(std::remove_if(_syncedObjects.begin(), _syncedObjects.end(),
                                      [](SyncedObject const& synced) { return !IsManagedAlive(synced.root); }),
                       _syncedObjects.end());
  if (_syncedObjects.empty()) return;

  bool const paused = _pauseMenuActive || !IsManagedAlive(_audioTimeSyncController);
  float const songTime = IsManagedAlive(_audioTimeSyncController) ? CurrentSongTime() : 0.0f;
  float const playbackSpeed = IsManagedAlive(_audioTimeSyncController)
                                  ? _audioTimeSyncController->get_timeScale()
                                  : 0.0f;
  // VideoPlayer exposes a seekable time directly, but Unity's legacy
  // AnimationState does not.  Re-sample captions/environment clips on the
  // first live frame and after a real timeline jump (practice start, seek or
  // restart) so they follow the same absolute song clock as video and notes.
  bool const timelineJumped =
      _lastSyncSongTime >= 0.0f &&
      (songTime + 0.25f < _lastSyncSongTime || songTime > _lastSyncSongTime + 0.5f);

  bool const heartbeat = [&]() {
    if (!GetVivifyDebugLogging()) return false;
    int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
    if (frame - _lastSyncHeartbeatFrame < 120) return false;
    _lastSyncHeartbeatFrame = frame;
    return true;
  }();

  for (auto& synced : _syncedObjects) {
    float const videoTime = std::max(0.0f, songTime - synced.startTime);
    float const animationTime = std::max(0.0f, songTime - synced.startTime);
    if (synced.needsInitialTimelineSample || timelineJumped) {
      for (auto* animation : synced.legacyAnimations) {
        auto* state = DefaultLegacyAnimationState(animation);
        if (state == nullptr || !UnityEngine::TrackedReference::op_Implicit_bool(state)) continue;
        state->set_time(animationTime);
        animation->Sample();
      }
      synced.needsInitialTimelineSample = false;
      if (timelineJumped) {
        VIVIFY_DEBUG("Vivify sync: sampled legacy animation timeline at songTime={} startTime={} elapsed={}",
                     songTime, synced.startTime, animationTime);
      }
    }
    for (auto* vp : synced.videoPlayers) {
      if (!IsManagedAlive(vp)) continue;
      if (paused) {
        if (vp->get_isPlaying()) {
          VIVIFY_DEBUG("Vivify sync: pausing video (songTime={})", songTime);
          vp->Pause();
        }
        continue;
      }
      if (MeaningfulRateChange(synced.lastVideoPlaybackSpeed, playbackSpeed)) {
        vp->set_playbackSpeed(playbackSpeed);
        synced.lastVideoPlaybackSpeed = playbackSpeed;
      }
      if (!vp->get_isPlaying()) {
        VIVIFY_DEBUG("Vivify sync: starting video (prepared={} songTime={} videoTime={})",
                     vp->get_isPrepared(), songTime, videoTime);
        vp->Play();
      }

      float const drift = static_cast<float>(vp->get_time()) - videoTime;
      if (std::abs(drift) > 0.15f) {
        vp->set_time(static_cast<double>(videoTime));
      }
      if (heartbeat) {
        PaperLogger.info("Vivify sync heartbeat: videoTime={} target={} songTime={} startTime={} drift={} speed={} prepared={} playing={}",
                         static_cast<float>(vp->get_time()), videoTime, songTime, synced.startTime,
                         drift, playbackSpeed, vp->get_isPrepared(), vp->get_isPlaying());
      }
    }
  }
}

bool Runtime::IsReduceDebrisEnabled() {

  if (!_reduceDebrisCached) {
    _reduceDebrisCached = true;
    _reduceDebris = false;
    auto* playerDataModel = UnityEngine::Object::FindObjectOfType<GlobalNamespace::PlayerDataModel*>();
    if (IsManagedAlive(playerDataModel)) {
      auto* playerData = playerDataModel->get_playerData();
      if (playerData != nullptr) {
        auto* settings = playerData->get_playerSpecificSettings();
        if (settings != nullptr) {
          _reduceDebris = settings->get_reduceDebris();
        }
      }
    }
    VIVIFY_DEBUG("Vivify: Reduce Debris player option = {} (custom debris {})", _reduceDebris,
                 _reduceDebris ? "disabled" : "enabled");
  }
  return _reduceDebris;
}

bool Runtime::IsLeftHanded() {
  if (_leftHandedCached) return _leftHanded;

  _leftHandedCached = true;
  _leftHanded = false;
  auto* playerDataModel = UnityEngine::Object::FindObjectOfType<GlobalNamespace::PlayerDataModel*>();
  if (IsAlive(playerDataModel)) {
    auto* playerData = playerDataModel->get_playerData();
    if (playerData != nullptr) {
      auto* settings = playerData->get_playerSpecificSettings();
      if (settings != nullptr) _leftHanded = settings->get_leftHanded();
    }
  }
  VIVIFY_DEBUG("Vivify prefab mirroring: left-handed mode={}", BoolText(_leftHanded));
  return _leftHanded;
}

UnityEngine::Transform* Runtime::EnsureMirroredPrefabParent() {
  if (!IsLeftHanded()) return nullptr;
  if (!IsAlive(_mirroredPrefabParent)) {
    _mirroredPrefabParent = UnityEngine::GameObject::New_ctor(u"VivifyLeftHandPrefabParent");
    if (!IsAlive(_mirroredPrefabParent)) return nullptr;
    _mirroredPrefabParent->get_transform()->set_localScale(UnityEngine::Vector3(-1.0f, 1.0f, 1.0f));
  }
  return _mirroredPrefabParent->get_transform().unsafePtr();
}

void Runtime::SetPauseMenuActive(bool active) {
  _pauseMenuRequested = active;
  ApplyPauseState();
}

void Runtime::SetApplicationPaused(bool paused) {
  _applicationPaused = paused;
  ApplyPauseState();
}

void Runtime::SetApplicationFocused(bool focused) {
  _applicationFocused = focused;
  ApplyPauseState();
}

void Runtime::ApplyPauseState() {
  bool const active = _pauseMenuRequested || _applicationPaused || !_applicationFocused;
  if (_currentBeatmapData == nullptr) {
    _pauseMenuActive = false;
    return;
  }
  if (_pauseMenuActive == active) return;
  _pauseMenuActive = active;
  VIVIFY_DEBUG("Vivify suspension {}: menu={} applicationPaused={} applicationFocused={} syncedObjects={} secondaryCameras={}",
               active ? "entered" : "left", _pauseMenuRequested, _applicationPaused,
               _applicationFocused, _syncedObjects.size(), _secondaryCameras.size());
  if (active) {
    // Suspend Unity-driven prefab time immediately. Runtime::Update stops as
    // soon as the lifecycle is suspended, so deferring this to
    // UpdatePrefabAnimationSpeed lets Animator and ParticleSystem components
    // keep advancing behind the pause or Quest system menu.
    for (auto& synced : _syncedObjects) {
      for (auto* animator : synced.animators) {
        if (IsManagedAlive(animator)) animator->set_speed(0.0f);
      }
      for (auto* animation : synced.legacyAnimations) {
        SetLegacyAnimationSpeed(animation, 0.0f);
      }
      for (auto* particleSystem : synced.particleSystems) {
        if (!IsManagedAlive(particleSystem)) continue;
        auto main = particleSystem->get_main();
        main.set_simulationSpeed(0.0f);
      }
      for (auto* vp : synced.videoPlayers) {
        if (IsManagedAlive(vp) && vp->get_isPlaying()) vp->Pause();
      }
    }
    _lastAppliedSyncSpeed = 0.0f;

    _lifecycle.Suspend();

    if (_cameraApplier != nullptr && UnityEngine::Object::op_Implicit_bool(_cameraApplier)) {
      _cameraApplier->set_enabled(false);
    }
    // A secondary camera can be disabled intentionally (for example when it
    // has no color/depth target). Remember that state exactly; blindly enabling
    // every camera on resume can render an un-targeted helper into the headset.
    RestoreSecondaryCullingLayers();
    RemoveMidRenderCommandBuffers();
    for (auto& [name, cam] : _secondaryCameras) {
      cam.enabledBeforePause = IsAlive(cam.camera) && cam.camera->get_enabled();
      if (IsAlive(cam.camera)) cam.camera->set_enabled(false);
    }

    RestoreGlobalProperties();

    // Keep authored render and QualitySettings values alive through the pause
    // menu. Restoring antiAliasing here and reapplying it immediately after an
    // Android XR resume can recreate the eye render targets while OpenXR is
    // reattaching its swapchain. The captured Aether freeze stopped on exactly
    // that resume path. Desktop Vivify also keeps these values until runtime
    // disposal, so teardown/restart remains the only place that restores them.
    VIVIFY_DEBUG("Vivify pause: preserving {} authored render setting(s) across XR suspend",
                 _currentRenderSettings.size());

    _pausedMaterialAnimations = std::move(_materialAnimations);
    _pausedGlobalAnimations = std::move(_globalAnimations);
    _pausedAnimatorAnimations = std::move(_animatorAnimations);
    _pausedRenderSettingAnimations = std::move(_renderSettingAnimations);
    _materialAnimations.clear();
    _globalAnimations.clear();
    _animatorAnimations.clear();
    _renderSettingAnimations.clear();
    // Sorting overrides are gameplay state, not an animation clock. Keep them
    // across pause so Continue cannot put notes back behind authored transparent
    // foreground geometry. ResetRuntime restores them when gameplay ends.
  } else {
    for (auto& [name, cam] : _secondaryCameras) {
      if (IsAlive(cam.camera)) cam.camera->set_enabled(cam.enabledBeforePause);
      cam.enabledBeforePause = false;
    }
    VIVIFY_DEBUG("Vivify resume checkpoint: secondary cameras restored");

    ReapplyCurrentGlobalProperties();
    VIVIFY_DEBUG("Vivify resume checkpoint: global shader state restored; authored render settings preserved");
    _materialAnimations = std::move(_pausedMaterialAnimations);
    _globalAnimations = std::move(_pausedGlobalAnimations);
    _animatorAnimations = std::move(_pausedAnimatorAnimations);
    _renderSettingAnimations = std::move(_pausedRenderSettingAnimations);
    _pausedMaterialAnimations.clear();
    _pausedGlobalAnimations.clear();
    _pausedAnimatorAnimations.clear();
    _pausedRenderSettingAnimations.clear();
    VIVIFY_DEBUG("Vivify resume checkpoint: authored animations restored");

    float resumeSpeed = 0.0f;
    if (IsManagedAlive(_audioTimeSyncController)) {
      float const authoredSpeed = _audioTimeSyncController->get_timeScale();
      if (std::isfinite(authoredSpeed) && authoredSpeed > 0.0f) resumeSpeed = authoredSpeed;
    }
    for (auto& synced : _syncedObjects) {
      for (auto* animator : synced.animators) {
        if (IsManagedAlive(animator)) animator->set_speed(resumeSpeed);
      }
      for (auto* animation : synced.legacyAnimations) {
        SetLegacyAnimationSpeed(animation, resumeSpeed);
      }
      for (auto* particleSystem : synced.particleSystems) {
        if (!IsManagedAlive(particleSystem)) continue;
        auto main = particleSystem->get_main();
        main.set_simulationSpeed(resumeSpeed);
      }
    }

    _lifecycle.Resume();
    // Force a fresh callback-clock sample and reapply the practice speed on
    // the first resumed frame. Otherwise a long Android sleep can leave
    // Animator/ParticleSystem components at the pause speed of zero.
    _songTimeCacheFrame = -1;
    _lastSyncSongTime = -1.0f;
    _lastAppliedSyncSpeed = std::numeric_limits<float>::quiet_NaN();
    VIVIFY_DEBUG("Vivify resume checkpoint: lifecycle active; refreshing camera components");
    if (!_isResetting) RefreshCameraComponents(true);
    VIVIFY_DEBUG("Vivify resume complete: songTime={} syncedObjects={} secondaryCameras={} renderSettingsPreserved={}",
                 CurrentSongTime(), _syncedObjects.size(), _secondaryCameras.size(),
                 _currentRenderSettings.size());
  }
}

void Runtime::HandleScenesWillDismiss() {
  if (_currentBeatmapData == nullptr && _lifecycle.Phase() == RuntimePhase::Dormant &&
      _activeSabers.empty() && _cameraApplier == nullptr &&
      _secondaryCameras.empty() && _livePrefabs.empty()) {
    return;
  }
  VIVIFY_DEBUG("Vivify retiring gameplay session before scene dismissal");
  ResetRuntime(ResetMode::EarlySceneTransition);
}

void Runtime::HandleGameplayRestart() {
  if (_currentBeatmapData == nullptr || _isResetting) return;
  VIVIFY_DEBUG("Vivify retiring gameplay session before restart");
  ResetRuntime(ResetMode::LiveScene);
}

bool Runtime::IsCameraApplierCurrent(CameraApplier* applier) const {
  if (!IsAlive(applier) || applier != _cameraApplier ||
      !_lifecycle.CanRender(applier->sessionGeneration)) {
    return false;
  }
  auto gameObject = applier->get_gameObject();
  return IsAlive(gameObject.unsafePtr()) && gameObject.unsafePtr() == _cameraApplierGO;
}

bool Runtime::BeginCameraRender(CameraApplier* applier) {
  return IsCameraApplierCurrent(applier) &&
         _lifecycle.TryEnterRender(applier->sessionGeneration);
}

void Runtime::EndCameraRender(CameraApplier* applier) {
  (void)applier;
  _lifecycle.LeaveRender();
  FinishPendingReset();
}

bool Runtime::ShouldSuppressOriginalImageEffect(
    GlobalNamespace::ImageEffectController* controller) const {
  return IsAlive(controller) && IsCameraApplierCurrent(_cameraApplier) &&
         _cameraApplier->get_enabled() && _cameraApplier->hasMainEffect &&
         _cameraApplier->imageEffectController == controller;
}

void Runtime::OnCameraApplierDestroyed(CameraApplier* applier) {
  RemoveMidRenderCommandBuffers(applier);
  if (_cameraApplier != applier) return;
  _cameraApplier = nullptr;
  _cameraApplierGO = nullptr;
}

void Runtime::HandleAlwaysVisibleQuad(GlobalNamespace::AlwaysVisibleQuad* quad) {
  if (_currentBeatmapData == nullptr || _isResetting || !IsAlive(quad)) return;
  auto* transform = quad->get_transform().unsafePtr();
  if (!IsAlive(transform)) return;
  if (!_alwaysVisibleQuadPositions.contains(transform)) {
    _alwaysVisibleQuadPositions.emplace(transform, transform->get_position());
  }
  auto position = transform->get_position();
  position.y = -1000.0f;
  transform->set_position(position);

  // Moving the quad is not sufficient on Quest: the HUD controller can write
  // its transform again after OnEnable, exposing the two transparent score
  // masks in maps such as 42-flux. Disable only renderers that were enabled so
  // reset can restore exactly the state Vivify owns.
  auto renderers = quad->GetComponentsInChildren<UnityEngine::Renderer*>(true);
  for (auto* renderer : renderers) {
    if (!IsAlive(renderer) || !renderer->get_enabled()) continue;
    _alwaysVisibleQuadDisabledRenderers.emplace(renderer);
    renderer->set_enabled(false);
  }
}

void Runtime::SuppressActiveAlwaysVisibleQuads() {
  if (_currentBeatmapData == nullptr || _isResetting) return;
  auto quads = UnityEngine::Object::FindObjectsOfType<GlobalNamespace::AlwaysVisibleQuad*>(true);
  for (auto* quad : quads) HandleAlwaysVisibleQuad(quad);
}

void Runtime::RestoreAlwaysVisibleQuads() {
  for (auto* renderer : _alwaysVisibleQuadDisabledRenderers) {
    if (IsAlive(renderer)) renderer->set_enabled(true);
  }
  _alwaysVisibleQuadDisabledRenderers.clear();
  for (auto const& [transform, position] : _alwaysVisibleQuadPositions) {
    if (IsAlive(transform)) transform->set_position(position);
  }
  _alwaysVisibleQuadPositions.clear();
}

void Runtime::RefreshMultipassRendering() {
  auto mainCam = UnityEngine::Camera::get_main();
  auto mainCamGO = IsAlive(mainCam.unsafePtr()) ? mainCam->get_gameObject().unsafePtr() : nullptr;
  RefreshMultipassRendering(mainCamGO);
  RefreshLoadedMaterialStereoKeywords();
}

void Runtime::RefreshIsolationSettings() {
  if (GetDisableAllBlits()) {
    if ((!_preEffects.empty() || !_postEffects.empty() || HasMidRenderEffects()) && GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify isolation: clearing active blits (Blit effects disabled) pre={} post={}",
                       _preEffects.size(), _postEffects.size());
    }
    _preEffects.clear();
    _postEffects.clear();
    for (auto& bucket : _midEffects) bucket.clear();
    DestroyCameraApplier();
    ReleaseMidRenderTextures();
    ReleaseCachedBlitTextures();
  } else if (GetDisableBeat0FilmgrainBlit()) {
    UpdateBlitEffects();
  }

  if (GetDisableCreateCameraDepth()) {
    if (!_secondaryCameras.empty() && GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify isolation: releasing secondary cameras count={}", _secondaryCameras.size());
    }
    for (auto& [name, camera] : _secondaryCameras) {
      ReleaseSecondaryCameraData(camera);
    }
    _secondaryCameras.clear();
    for (auto it = _cameraProperties.begin(); it != _cameraProperties.end();) {
      if (it->first != kMainCameraId) {
        it = _cameraProperties.erase(it);
      } else {
        it++;
      }
    }
  } else {
    // Re-apply stored secondary-camera properties after an isolation setting
    // changes. The culling controller owns temporary layer restoration.
    for (auto& [name, camera] : _secondaryCameras) {
      if (!IsAlive(camera.camera)) continue;
      ApplySecondaryCameraProperties(camera);
      ApplyCameraGameObjectProperties(camera.camera, camera.properties);
    }
  }

  RefreshCameraComponents(_currentBeatmapData != nullptr && !_isResetting && !_pauseMenuActive);
}

void Runtime::ResetRuntime(ResetMode mode) {
  auto const resetRank = [](ResetMode candidate) {
    switch (candidate) {
      case ResetMode::LiveScene: return 0;
      case ResetMode::EarlySceneTransition: return 1;
      case ResetMode::LateSceneTransition: return 2;
    }
    return 2;
  };
  auto const resetName = [](ResetMode candidate) -> char const* {
    switch (candidate) {
      case ResetMode::LiveScene: return "live-scene";
      case ResetMode::EarlySceneTransition: return "early-transition";
      case ResetMode::LateSceneTransition: return "late-transition";
    }
    return "unknown";
  };

  [[maybe_unused]] auto const retiredGeneration = _lifecycle.BeginRetirement();
  if (_lifecycle.RenderDepth() != 0) {
    if (!_pendingResetMode.has_value() ||
        resetRank(mode) > resetRank(_pendingResetMode.value())) {
      _pendingResetMode = mode;
    }
    _isResetting = true;
    VIVIFY_DEBUG("Vivify deferred {} reset until {} active render scope(s) leave",
                 resetName(mode), _lifecycle.RenderDepth());
    return;
  }

  VIVIFY_DEBUG("Vivify ResetRuntime: mode={} livePrefabs={} preloaded={} synced={} secondaryCameras={} declaredTextures={} assets={}",
               resetName(mode), _livePrefabs.size(), _instantiatePrefabs.size(), _syncedObjects.size(),
               _secondaryCameras.size(), _declaredTextures.size(), _assets.size());
  _isResetting = true;
  bool const canTouchSceneObjects = mode != ResetMode::LateSceneTransition;
  bool const preserveGameplayScene = mode == ResetMode::LiveScene;

  if (canTouchSceneObjects) {
    // This path is used while the gameplay scene is still alive (for example a
    // practice-mode backward seek), so authored state can be restored normally.
    DestroyCameraApplier();
    RestoreGlobalProperties();
    RestoreAllVisualReplacements();
    RestoreOverlayRenderState();
    RestoreAlwaysVisibleQuads();
    RestoreMainCameraOriginals();

    std::unordered_set<UnityEngine::GameObject*> destroyed;
    for (auto& [id, prefab] : _livePrefabs) {
      if (prefab.gameObject == nullptr) continue;
      for (auto const& track : prefab.tracks) track.UnregisterGameObject(prefab.gameObject);
      if (destroyed.emplace(prefab.gameObject).second && IsAlive(prefab.gameObject)) {
        UnityEngine::Object::Destroy(prefab.gameObject);
      }
    }
    for (auto& [eventData, instantiate] : _instantiatePrefabs) {
      if (instantiate.instance != nullptr && destroyed.emplace(instantiate.instance).second &&
          IsAlive(instantiate.instance)) {
        UnityEngine::Object::Destroy(instantiate.instance);
      }
      instantiate.instance = nullptr;
    }
    if (IsAlive(_mirroredPrefabParent)) {
      UnityEngine::Object::Destroy(_mirroredPrefabParent);
    }

    for (auto& [name, dt] : _declaredTextures) ReleaseDeclaredTextureData(dt);
    for (auto& [name, cam] : _secondaryCameras) ReleaseSecondaryCameraData(cam);
    RemoveMidRenderCommandBuffers();
    ReleaseMidRenderTextures();
    ReleaseCachedBlitTextures();
    RestoreRenderSettings();

    if (IsAlive(_multipassController)) {
      _multipassController->set_enabled(false);
      UnityEngine::Object::Destroy(_multipassController);
    }
  } else {
    // Scene transitions are deliberately pointer-free. Crash tombstones showed
    // stale wrappers reaching RenderTexture::Release, Object::Destroy,
    // Renderer.enabled, and Behaviour.enabled after leaving a song. Only static
    // shader bindings are cleared here; Unity owns destruction of the old scene.
    for (auto const& [name, data] : _declaredTextures) {
      UnityEngine::Shader::SetGlobalTexture(data.propertyId, static_cast<UnityEngine::Texture*>(nullptr));
    }
    for (auto const& [name, data] : _secondaryCameras) {
      if (data.texturePropertyId.has_value()) {
        UnityEngine::Shader::SetGlobalTexture(data.texturePropertyId.value(), static_cast<UnityEngine::Texture*>(nullptr));
      }
      if (data.depthTexturePropertyId.has_value()) {
        UnityEngine::Shader::SetGlobalTexture(data.depthTexturePropertyId.value(), static_cast<UnityEngine::Texture*>(nullptr));
      }
    }
    for (auto const& [propertyId, value] : _currentGlobalProperties) {
      if (std::holds_alternative<UnityEngine::Texture*>(value)) {
        UnityEngine::Shader::SetGlobalTexture(propertyId, static_cast<UnityEngine::Texture*>(nullptr));
      }
    }
    for (auto const& [keyword, enabled] : _savedGlobalKeywords) {
      if (enabled) UnityEngine::Shader::EnableKeyword(StringW(keyword));
      else UnityEngine::Shader::DisableKeyword(StringW(keyword));
    }

    // Do not Dispose/Destroy transition-owned native objects. Clear every raw
    // pointer so the persistent VivifyRuntime behaviour cannot touch them on a
    // later menu frame or when another song is selected.
    _cameraApplier = nullptr;
    _cameraApplierGO = nullptr;
    _multipassController = nullptr;
    _lastMainCameraGO = nullptr;
    _midCommandBufferCamera = nullptr;
    _midCommandBufferOwner = nullptr;
    _activeMidCommandBuffers.clear();
    _midSelfBlitTemps.clear();
    _midRenderCommandCache.Invalidate();
    _midMainRT = nullptr;
    _midScratchRT = nullptr;
    _mainBlitTexture = nullptr;
    _scratchBlitTexture = nullptr;
    ReleaseImageBlitCommandBuffer(false);
    SetMultipassShaderState(false);
    _alwaysVisibleQuadPositions.clear();
    _alwaysVisibleQuadDisabledRenderers.clear();
    _beatmapCallbacksController = nullptr;
  }

  _livePrefabs.clear();
  _instantiatePrefabs.clear();
  _deferredPrefabPrewarms.clear();
  _syncedObjects.clear();
  _blitMaterialValidCache.clear();
  _warnedInvalidBlitPasses.clear();
  _loggedBlitStereoMaterials.clear();
  _materialAnimations.clear();
  _globalAnimations.clear();
  _animatorAnimations.clear();
  _renderSettingAnimations.clear();
  _pausedMaterialAnimations.clear();
  _pausedGlobalAnimations.clear();
  _pausedAnimatorAnimations.clear();
  _pausedRenderSettingAnimations.clear();
  _savedGlobalProperties.clear();
  _savedGlobalKeywords.clear();
  _currentGlobalProperties.clear();
  _currentGlobalKeywords.clear();
  _repairedMaterials.clear();
  _genericFallbackMaterials.clear();
  _assets.clear();
  _assetsByName.clear();
  _ambiguousAssetAliases.clear();
  _missingAssetKeys.clear();
  _missingInstantiatePrefabAssets.clear();
  _supportedShadersByName.clear();
  _catchUpAppliedCustomEvents.clear();
  _deferredStartupPostProcessingEvents.clear();
  _deferredSaberVisualsUntilFrame = -1;
  _saberVisualRetryUntilFrame = -1;
  _lastSaberIntegrityFrame = -1000;
  _warnedInvalidBeatmapData = false;
  _warnedStartupDescriptorWait = false;
  _postProcessingReadyFrame = -1;
  _nextBeatmapPrepareProbeFrame = 0;
  _nextAlwaysVisibleQuadScanFrame = 0;
  _declaredTextures.clear();
  _secondaryCameras.clear();
  _preEffects.clear();
  _postEffects.clear();
  for (auto& bucket : _midEffects) bucket.clear();
  _midRenderCommandCache.Invalidate();
  _hasMainDescriptor = false;
  _cachedMainVrUsage = -1;
  _savedRenderSettings.clear();
  _currentRenderSettings.clear();
  _assignedPrefabs.clear();
  InvalidateAssignedPrefabLookup();
  _activeDebrisPrefabStack.clear();
  _lastCutDebrisPrefabs.clear();
  if (!preserveGameplayScene) {
    _activeSabers.clear();
    _activeNoteControllers.clear();
  } else {
    // PrepareBeatmap and practice/restart resets run while the same gameplay
    // scene (and SaberModelController objects) are still alive. Erasing this
    // list made AssignObjectPrefab at beat 0 a no-op until pausing/restarting
    // happened to reconstruct the saber hierarchy.
    PurgeInvalidActiveSabers();
    for (auto it = _activeNoteControllers.begin(); it != _activeNoteControllers.end();) {
      if (!IsAlive(*it)) {
        it = _activeNoteControllers.erase(it);
      } else {
        ++it;
      }
    }
  }
  _noteReplacements.clear();
  _saberReplacements.clear();
  _debrisReplacements.clear();
  _overlayRendererSortingOrders.clear();
  _alwaysVisibleQuadPositions.clear();
  _alwaysVisibleQuadDisabledRenderers.clear();
  _cameraProperties.clear();
  _mainCameraPropsDirty = false;
  _mainCamOriginalDepthMode.reset();
  _mainCamOriginalClearFlags.reset();
  _mainCamOriginalBackgroundColor.reset();
  _mirroredPrefabParent = nullptr;

  if (preserveGameplayScene && _mainBundle != nullptr && !UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
    _mainBundle = nullptr;
    _preloadedBundlePath.clear();
  }
  if (!preserveGameplayScene) _beatmapCallbacksController = nullptr;
  _currentBeatmapData = nullptr;
  _beatmapAD = nullptr;
  _audioTimeSyncController = nullptr;
  _lastSongTime = -1.0f;
  _lastSyncSongTime = -1.0f;
  _lastAppliedSyncSpeed = std::numeric_limits<float>::quiet_NaN();
  _lastQuestPerformanceFrame = -1000;
  _midCommandBufferCacheHits = 0;
  _midCommandBufferRebuilds = 0;
  _imageBlitExecutions = 0;
  _imageCommandBufferCreates = 0;
  _noteRefreshEvents = 0;
  _noteRefreshCandidates = 0;
  _assignedPrefabLookupRebuilds = 0;
  _songTimeCacheFrame = -1;
  _songTimeCache = 0.0f;
  _pauseMenuRequested = false;
  _applicationPaused = false;
  _applicationFocused = true;
  _pauseMenuActive = false;
  _reduceDebrisCached = false;
  _reduceDebris = false;
  _leftHandedCached = false;
  _leftHanded = false;
  _preparingGeneration = 0;
  _pendingResetMode.reset();
  [[maybe_unused]] bool const retired = _lifecycle.CompleteRetirement();
  _isResetting = false;
}

void Runtime::FinishPendingReset() {
  if (_lifecycle.RenderDepth() != 0 || !_pendingResetMode.has_value()) return;
  auto const mode = _pendingResetMode.value();
  _pendingResetMode.reset();
  _isResetting = false;
  ResetRuntime(mode);
}

}
