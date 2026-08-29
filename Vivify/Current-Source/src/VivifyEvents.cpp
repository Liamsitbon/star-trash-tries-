#include "VivifyRuntimeInternal.hpp"
#include "VivifyComponents.hpp"

namespace Vivify {

rapidjson::Value const* Runtime::GetEventJson(CustomJSONData::CustomEventData* customEventData) const {
  if (customEventData->customData == nullptr || !customEventData->customData->value.has_value()) {
    return nullptr;
  }
  return &customEventData->customData->value.value().get();
}

std::optional<PointDefinitionW> Runtime::MakePointDefinition(rapidjson::Value const& object,
                                                             std::string_view key,
                                                             Tracks::ffi::WrapBaseValueType type) {
  auto* value = ReadValuePtr(object, key);
  if (value == nullptr) {
    return std::nullopt;
  }
  if (!value->IsArray() && !value->IsString()) {
    return std::nullopt;
  }
  if (_beatmapAD != nullptr) {
    return _beatmapAD->getPointDefinition(object, key, type);
  }
  if (!value->IsArray()) {
    return std::nullopt;
  }
  return PointDefinitionW(*value, type, _fallbackBeatmapAD.GetBaseProviderContext());
}

std::vector<TrackW> Runtime::ReadTracks(rapidjson::Value const& object, bool v2) {
  if (_beatmapAD == nullptr) {
    return {};
  }
  auto trackKey = v2 ? TracksAD::Constants::V2_TRACK : TracksAD::Constants::TRACK;
  auto tracks = NEJSON::ReadOptionalTracks(object, trackKey, *_beatmapAD);
  if (!tracks.has_value()) {
    return {};
  }
  return {tracks->begin(), tracks->end()};
}

CustomJSONData::CustomBeatmapData* Runtime::GetCustomBeatmapData(GlobalNamespace::BeatmapCallbacksController* callbackController) {
  if (callbackController == nullptr) return nullptr;
  try {
    if (callbackController->_beatmapData == nullptr) return nullptr;
    auto* result = il2cpp_utils::try_cast<CustomJSONData::CustomBeatmapData>(
                       callbackController->_beatmapData)
                       .value_or(nullptr);
    if (result == nullptr && !_warnedInvalidBeatmapData) {
      _warnedInvalidBeatmapData = true;
      PaperLogger.warn(
          "Vivify gameplay callback has non-custom beatmap data; Vivify events will be ignored");
    }
    return result;
  } catch (std::exception const& ex) {
    if (!_warnedInvalidBeatmapData) {
      _warnedInvalidBeatmapData = true;
      PaperLogger.warn("Vivify could not inspect gameplay beatmap data: {}", ex.what());
    }
  } catch (...) {
    if (!_warnedInvalidBeatmapData) {
      _warnedInvalidBeatmapData = true;
      PaperLogger.warn("Vivify could not inspect gameplay beatmap data");
    }
  }
  return nullptr;
}

bool Runtime::EnsureBeatmapPrepared(GlobalNamespace::BeatmapCallbacksController* callbackController, float triggerTime) {
  auto* customBeatmapData = GetCustomBeatmapData(callbackController);
  if (customBeatmapData == nullptr) return false;
  if (_currentBeatmapData == customBeatmapData) {
    _beatmapCallbacksController = callbackController;
    return true;
  }
  PrepareBeatmap(customBeatmapData, triggerTime, callbackController);
  return _currentBeatmapData == customBeatmapData;
}

void Runtime::PrepareBeatmap(
    CustomJSONData::CustomBeatmapData* beatmapData, float triggerTime,
    GlobalNamespace::BeatmapCallbacksController* callbackController) {
  if (beatmapData == nullptr) return;
  ResetRuntime();
  _preparingGeneration = _lifecycle.BeginPreparation();
  _currentBeatmapData = beatmapData;
  _beatmapCallbacksController = callbackController;
  _warnedInvalidBeatmapData = false;
  // Keep every beat-0 effect registered, but give the first XR render a small
  // head start.  The old 18-frame/two-frames-per-event throttle made maps such
  // as 42-flux start with filmgrain and screen-texture passes missing, then
  // appear to "come back" after the first notes.  The queue is still drained
  // only after a real XR descriptor is known, so allocations never use the
  // desktop camera dimensions.
  _postProcessingReadyFrame = IsQuestXrRuntime()
                                  ? static_cast<int>(UnityEngine::Time::get_frameCount()) + 2
                                  : -1;
  _beatmapAD = nullptr;
  _audioTimeSyncController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
  if (beatmapData->customData != nullptr) {
    try {
      TracksAD::readBeatmapDataAD(beatmapData);
      _beatmapAD = &TracksAD::getBeatmapAD(beatmapData->customData);
    } catch (std::exception const& ex) {
      PaperLogger.warn("Vivify PrepareBeatmap: readBeatmapDataAD threw: {}", ex.what());
    }
  }
  VIVIFY_DEBUG("Vivify PrepareBeatmap: v2={} customEvents={} triggerTime={} beatmapAD={}",
               beatmapData->v2orEarlier, beatmapData->customEventDatas.size(),
               triggerTime, _beatmapAD != nullptr);
  LoadMainBundle();
  PreloadInstantiatePrefabs();
  ApplyMissedVivifyEvents(triggerTime);
  SuppressActiveAlwaysVisibleQuads();
  _lastSongTime = CurrentSongTime();
  if (!_lifecycle.Activate(_preparingGeneration)) {
    PaperLogger.error("Vivify discarded beatmap preparation because its runtime generation was retired");
    ResetRuntime(ResetMode::LateSceneTransition);
    return;
  }
  _preparingGeneration = 0;
  RefreshCameraComponents(true);
  VIVIFY_DEBUG("Vivify PrepareBeatmap done: assets={} livePrefabs={} preloadedPrefabs={}",
               _assets.size(), _livePrefabs.size(), _instantiatePrefabs.size());
}

void Runtime::DispatchEvent(std::string_view type, CustomJSONData::CustomEventData* customEventData,
                            rapidjson::Value const& json) {

  float const eventTime = customEventData != nullptr ? customEventData->time : -1.0f;
  try {
    if (type == kInstantiatePrefabEvent) {
      InstantiatePrefab(customEventData, json);
    } else if (type == kDestroyObjectEvent) {
      DestroyObjects(json);
    } else if (type == kSetMaterialPropertyEvent) {
      HandleSetMaterialProperty(customEventData, json);
    } else if (type == kSetAnimatorPropertyEvent) {
      HandleSetAnimatorProperty(customEventData, json);
    } else if (type == kSetGlobalPropertyEvent) {
      HandleSetGlobalProperty(customEventData, json);
    } else if (type == kBlitEvent) {
      HandleBlit(customEventData, json);
    } else if (type == kCreateCameraEvent) {
      HandleCreateCamera(json);
    } else if (type == kCreateScreenTextureEvent) {
      HandleCreateScreenTexture(json);
    } else if (type == kSetCameraPropertyEvent) {
      HandleSetCameraProperty(json);
    } else if (type == kSetRenderingSettingsEvent) {
      HandleSetRenderingSettings(customEventData, json);
    } else if (type == kAssignObjectPrefabEvent) {
      HandleAssignObjectPrefab(customEventData, json);
    }
  } catch (std::exception const& ex) {
    PaperLogger.error("Vivify event '{}' (time={}) threw and was skipped: {}",
                      std::string(type), eventTime, ex.what());
  } catch (...) {
    PaperLogger.error("Vivify event '{}' (time={}) threw a non-std exception and was skipped",
                      std::string(type), eventTime);
  }
}

bool Runtime::QueueStartupPostProcessingEvent(
    std::string_view type,
    CustomJSONData::CustomEventData* customEventData) {
  if (!IsQuestXrRuntime() || customEventData == nullptr ||
      (type != kCreateScreenTextureEvent && type != kBlitEvent)) {
    return false;
  }
  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  if (_postProcessingReadyFrame < frame) return false;

  _deferredStartupPostProcessingEvents.emplace_back(customEventData);
  VIVIFY_DEBUG("Vivify queued startup post-processing: type='{}' time={} readyFrame={}",
               std::string(type), customEventData->time, _postProcessingReadyFrame);
  return true;
}

void Runtime::UpdateStartupPostProcessingEvents() {
  if (_deferredStartupPostProcessingEvents.empty()) return;

  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  if (frame < _postProcessingReadyFrame) return;

  // Quest renders the gameplay camera into a stereo texture array. Do not
  // allocate a beat-0 screen texture from Camera.pixelWidth/pixelHeight before
  // OnRenderImage has exposed the real XR descriptor (dimensions, array depth,
  // graphics format and VR usage). RefreshCameraApplier keeps the callback
  // enabled while this startup queue is pending.
  if (!_hasMainDescriptor) {
    if (!_warnedStartupDescriptorWait) {
      _warnedStartupDescriptorWait = true;
      PaperLogger.info("Vivify startup post-processing is waiting for the first XR render descriptor");
    }
    return;
  }
  _warnedStartupDescriptorWait = false;

  // Preserve chart order (screen texture before its blit), while avoiding a
  // long visible gap at song start.  Beat-0 chains are normally only a few
  // declarations/passes; cap the work per frame so a malformed map cannot
  // turn this into an unbounded allocation burst.
  constexpr int kMaxStartupEventsPerFrame = 8;
  int dispatched = 0;
  while (!_deferredStartupPostProcessingEvents.empty() &&
         dispatched < kMaxStartupEventsPerFrame) {
    auto* customEventData = _deferredStartupPostProcessingEvents.front();
    _deferredStartupPostProcessingEvents.erase(_deferredStartupPostProcessingEvents.begin());
    if (customEventData == nullptr) continue;
    auto* json = GetEventJson(customEventData);
    if (json == nullptr) continue;
    DispatchEvent(customEventData->type, customEventData, *json);
    ++dispatched;
  }

  _postProcessingReadyFrame =
      _deferredStartupPostProcessingEvents.empty() ? -1 : frame + 1;
  VIVIFY_DEBUG("Vivify drained {} startup post-processing event(s); remaining={} nextFrame={}",
               dispatched, _deferredStartupPostProcessingEvents.size(),
               _postProcessingReadyFrame);
}

void Runtime::HandleCustomEvent(GlobalNamespace::BeatmapCallbacksController* callbackController,
                                CustomJSONData::CustomEventData* customEventData) {
  if (callbackController == nullptr || customEventData == nullptr) return;

  if (!_selectedMapHasVivifyRequirement) return;

  // This is intentionally checked before map preparation. It prevents the
  // asset loader, prefabs, cameras and command buffers from executing on Quest
  // while preserving ordinary Beat Saber gameplay for the chart.
  if (GetQuestCompatibilityMode()) {
    return;
  }

  // Synapse can deliberately start a LAN event without a downloaded Android
  // bundle after its bounded fallback timeout. Do not partially execute camera
  // and texture events in that state: desktop-authored events without their
  // matching assets are a common source of black/frozen Quest frames.
  if (_mainBundle == nullptr || !UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
    if (!_warnedMissingBundleEvents) {
      _warnedMissingBundleEvents = true;
      PaperLogger.warn("Vivify skipped custom events because no compatible Android asset bundle is loaded");
    }
    return;
  }

  if (!EnsureBeatmapPrepared(callbackController, customEventData->time)) return;
  std::string_view type = customEventData->type;
  if (!IsVivifyEvent(type)) return;

  if (_catchUpAppliedCustomEvents.erase(customEventData) > 0) return;
  if (QueueStartupPostProcessingEvent(type, customEventData)) return;
  auto* json = GetEventJson(customEventData);
  if (json == nullptr) return;
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify custom event: type='{}' time={} songTime={}",
                     std::string(type), customEventData->time, CurrentSongTime());
  }
  DispatchEvent(type, customEventData, *json);
}

void Runtime::ApplyMissedVivifyEvents(float upToTime) {
  if (_currentBeatmapData == nullptr) return;
  _catchUpAppliedCustomEvents.clear();

  // Reconstruct only the InstantiatePrefab events that are still alive at the
  // requested practice/seek time. Replaying every historical instantiate and
  // destroy in one frame creates a large transient allocation burst on Quest.
  std::unordered_map<std::string, CustomJSONData::CustomEventData*> livePrefabEvents;
  std::unordered_set<CustomJSONData::CustomEventData*> anonymousPrefabEvents;
  for (auto* customEventData : _currentBeatmapData->customEventDatas) {
    if (customEventData == nullptr || customEventData->time > upToTime) continue;
    std::string_view type = customEventData->type;
    auto* json = GetEventJson(customEventData);
    if (json == nullptr) continue;

    if (type == kInstantiatePrefabEvent) {
      if (auto id = ReadStringView(*json, "id"); id.has_value()) {
        livePrefabEvents[std::string(*id)] = customEventData;
      } else {
        anonymousPrefabEvents.emplace(customEventData);
      }
    } else if (type == kDestroyObjectEvent) {
      for (auto const& id : ReadStringListOrSingle(*json, "id")) {
        livePrefabEvents.erase(id);
      }
    }
  }

  auto shouldReplayPrefab = [&](CustomJSONData::CustomEventData* eventData,
                                rapidjson::Value const& json) {
    if (auto id = ReadStringView(json, "id"); id.has_value()) {
      auto found = livePrefabEvents.find(std::string(*id));
      return found != livePrefabEvents.end() && found->second == eventData;
    }
    return anonymousPrefabEvents.contains(eventData);
  };

  int applied = 0;
  int skippedHistoricalPrefabs = 0;
  int skippedHistoricalSingleFrameBlits = 0;
  for (auto* customEventData : _currentBeatmapData->customEventDatas) {
    if (customEventData == nullptr) continue;
    if (customEventData->time > upToTime) continue;
    std::string_view type = customEventData->type;
    if (!IsVivifyEvent(type)) continue;

    // Mark every historical event as consumed so BeatmapCallbacks cannot
    // deliver it a second time after the catch-up pass.
    _catchUpAppliedCustomEvents.emplace(customEventData);

    auto* json = GetEventJson(customEventData);
    if (json == nullptr) continue;

    if (type == kInstantiatePrefabEvent &&
        !shouldReplayPrefab(customEventData, *json)) {
      skippedHistoricalPrefabs++;
      continue;
    }

    // A zero-duration Blit is defined by desktop Vivify as a one-render-frame
    // effect.  Replaying every old one during a practice seek stacks unrelated
    // fullscreen materials in the same Quest frame and can submit already
    // obsolete GPU work.  Stateful events still replay below; only an
    // instantaneous Blit whose authored frame has passed is discarded.
    if (type == kBlitEvent &&
        customEventData->time + 0.01f < upToTime &&
        ReadFloat(*json, "duration").value_or(0.0f) == 0.0f) {
      skippedHistoricalSingleFrameBlits++;
      continue;
    }

    // At a normal beat-0 start, use the short XR-descriptor-aware startup
    // queue.  At a later seek time, dispatch historical post-processing state
    // now so expired effects are discarded immediately instead of rebuilding
    // a visible fullscreen gap.
    bool const atStartBoundary =
        std::fabs(customEventData->time - upToTime) < 0.01f;
    if (atStartBoundary &&
        QueueStartupPostProcessingEvent(type, customEventData)) {
      applied++;
      continue;
    }

    VIVIFY_DEBUG("Vivify catch-up event: type='{}' time={}", std::string(type), customEventData->time);
    DispatchEvent(type, customEventData, *json);
    applied++;
  }
  VIVIFY_DEBUG("Vivify catch-up: replayed {} event(s), skipped {} historical prefab spawn(s) and {} expired single-frame blit(s), up to time={}",
               applied, skippedHistoricalPrefabs,
               skippedHistoricalSingleFrameBlits, upToTime);
}

}
