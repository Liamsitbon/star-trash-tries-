#include "Constants.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/IJumpOffsetYProvider.hpp"
#include "GlobalNamespace/PlayerHeightDetector.hpp"
#include "GlobalNamespace/PlayerHeightToJumpOffsetYProvider.hpp"
#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/BeatmapDataObstaclesMergingTransform.hpp"
#include "GlobalNamespace/BeatmapDataTransformHelper.hpp"
#include "GlobalNamespace/BeatmapLineData.hpp"
#include "GlobalNamespace/EnvironmentEffectsFilterPreset.hpp"
#include "GlobalNamespace/EnvironmentIntensityReductionOptions.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "System/Collections/Generic/IEnumerable_1.hpp"
#include "System/Func_2.hpp"
#include "System/Linq/Enumerable.hpp"
#include "System/Linq/IOrderedEnumerable_1.hpp"
#include "UnityEngine/Resources.hpp"

#include "GlobalNamespace/BeatmapDataBasicInfo.hpp"

#include "AssociatedData.h"
#include "FakeNoteHelper.h"
#include "NEHooks.h"
#include "NELogger.h"
#include "NECaches.h"
#include "SceneTransitionHelper.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "custom-json-data/shared/CustomBeatmapSaveDatav3.h"
#include "GlobalNamespace/SortedList_1.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "SpawnDataHelper.h"
#include "custom-json-data/shared/JsonUtils.h"
#include "tracks/shared/AssociatedData.h"
#include "tracks/shared/Json.h"

#include <optional>

using namespace GlobalNamespace;
using namespace System::Collections::Generic;

//
// IReadonlyBeatmapData *ReorderLineData(IReadonlyBeatmapData *beatmapData) {
//    auto *customBeatmapData = static_cast<CustomJSONData::CustomBeatmapData *>(beatmapData->GetCopy());
//    if (!customObstacleDataClass) {
//        customObstacleDataClass = classof(CustomJSONData::CustomObstacleData *);
//        customNoteDataClass = classof(CustomJSONData::CustomNoteData *);
//    }
//
//
//
//    auto notes = customBeatmapData->GetBeatmapItemsCpp<NoteData*>();
//    auto obstacles = customBeatmapData->GetBeatmapItemsCpp<ObstacleData*>();
//
//    std::vector<BeatmapObjectData*> objects;
//    objects.reserve(notes.size() + obstacles.size());
//
//    std::copy(notes.begin(), notes.end(), std::back_inserter(objects));
//    std::copy(obstacles.begin(), obstacles.end(), std::back_inserter(objects));
//
//    // loop through all objects in all lines of the beatmapData
//    for (BeatmapObjectData *beatmapObjectData : objects) {
//
//    }
//
//    return reinterpret_cast<IReadonlyBeatmapData *>(customBeatmapData);
//}

extern System::Collections::Generic::LinkedList_1<BeatmapDataItem*>*
SortAndOrderList(CustomJSONData::CustomBeatmapData* beatmapData);

void LoadNoodleObjects(CustomJSONData::CustomBeatmapData* beatmap) {
  NELogger::Logger.info("BeatmapData klass name is {}", beatmap->klass->name);

  static auto* customObstacleDataClass = classof(CustomJSONData::CustomObstacleData*);
  static auto* customNoteDataClass = classof(CustomJSONData::CustomNoteData*);
  static auto* customSliderDataClass = classof(CustomJSONData::CustomSliderData*);

  TracksAD::BeatmapAssociatedData& beatmapAD = TracksAD::getBeatmapAD(beatmap->customData);

  if (!beatmapAD.valid) {
    TracksAD::readBeatmapDataAD(beatmap);
  }

  auto v2 = beatmap->v2orEarlier;

  bool mirror = true;

  if (beatmap->customData->value) {
    rapidjson::Value const& data = *beatmap->customData->value;
    mirror = NEJSON::ReadOptionalBool(data, v2 ? "_questNoteMirror" : "questNoteMirror").value_or(true);
  }

  for (BeatmapObjectData* beatmapObjectData : beatmap->beatmapObjectDatas) {
    CustomJSONData::CustomNoteData* noteData =
        il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(beatmapObjectData).value_or(nullptr);
    CustomJSONData::CustomObstacleData* obstacleData =
        il2cpp_utils::try_cast<CustomJSONData::CustomObstacleData>(beatmapObjectData).value_or(nullptr);
    CustomJSONData::CustomSliderData* sliderData =
        il2cpp_utils::try_cast<CustomJSONData::CustomSliderData>(beatmapObjectData).value_or(nullptr);
    CustomJSONData::JSONWrapper* customDataWrapper = nullptr;
    if (obstacleData) {
      customDataWrapper = obstacleData->customData;
    }
    if (noteData) {
      customDataWrapper = noteData->customData;
    }
    if (sliderData) {
      customDataWrapper = sliderData->customData;
    }

    if (!customDataWrapper) continue;

    BeatmapObjectAssociatedData& ad = getAD(customDataWrapper);

    ad.mirror = mirror;

    if (customDataWrapper->value) {
      rapidjson::Value const& customData = *customDataWrapper->value;

      if (ad.parsed) continue;

      auto v3Fake = ad.objectData.fake;
      ad.objectData = ObjectCustomData(customData, noteData, obstacleData, v2);
      if (v3Fake) ad.objectData.fake = v3Fake;

      if (!ad.flipX || !ad.flipY) {
        auto [x, y] = NEJSON::ReadOptionalPair(customData, v2 ? NoodleExtensions::Constants::V2_FLIP
                                                              : NoodleExtensions::Constants::FLIP);
        ad.flipX = x;
        ad.flipY = y;
      }

      auto animationKey = v2 ? NoodleExtensions::Constants::V2_ANIMATION : NoodleExtensions::Constants::ANIMATION;
      if (customData.HasMember(animationKey.data())) {
        rapidjson::Value const& animation = customData[animationKey.data()];
        ad.animationData = { beatmapAD, animation, v2 };
      } else {
        ad.animationData = AnimationObjectData();
      }
    }
    ad.parsed = true;
  }
}

void LoadNoodleEvent(TracksAD::BeatmapAssociatedData& beatmapAD, CustomJSONData::CustomEventData const* customEventData,
                     bool v2) {
  if (!customEventData || !customEventData->customData) return;

  bool isType = false;

  auto typeHash = customEventData->typeHash;

#define TYPE_GET(jsonName, varName)                                                                                    \
  static auto jsonNameHash_##varName = std::hash<std::string_view>()(jsonName);                                        \
  if (!isType && typeHash == (jsonNameHash_##varName)) isType = true;

  TYPE_GET(NoodleExtensions::Constants::ASSIGN_TRACK_PARENT, AssignTrackParent)
  TYPE_GET(NoodleExtensions::Constants::ASSIGN_PLAYER_TO_TRACK, AssignPlayerToTrack)

  if (!isType) {
    return;
  }

  auto& eventAD = getEventAD(customEventData);

  if (!customEventData->customData->value) {
    NELogger::Logger.warn("Skipping malformed Noodle custom event: missing object customData");
    eventAD.parsed = true;
    return;
  }

  rapidjson::Value const& eventData = *customEventData->customData->value;
  if (!eventData.IsObject()) {
    NELogger::Logger.warn("Skipping malformed Noodle custom event: customData was not an object");
    eventAD.parsed = true;
    return;
  }

  if (eventAD.parsed) return;

  if (typeHash == jsonNameHash_AssignTrackParent) {
    auto parentTrackKey = v2 ? NoodleExtensions::Constants::V2_PARENT_TRACK.data()
                             : NoodleExtensions::Constants::PARENT_TRACK.data();
    auto parentTrackIt = eventData.FindMember(parentTrackKey);
    if (parentTrackIt == eventData.MemberEnd() || !parentTrackIt->value.IsString()) {
      NELogger::Logger.warn("Skipping AssignTrackParent without a valid parent track");
      eventAD.parsed = true;
      return;
    }
    eventAD.parentTrackEventData.emplace(eventData, beatmapAD, v2);
  } else if (typeHash == jsonNameHash_AssignPlayerToTrack) {
    auto trackKey = v2 ? NoodleExtensions::Constants::V2_TRACK.data() : NoodleExtensions::Constants::TRACK.data();
    auto trackIt = eventData.FindMember(trackKey);
    if (trackIt == eventData.MemberEnd() || !trackIt->value.IsString()) {
      NELogger::Logger.warn("Skipping AssignPlayerToTrack without a valid track");
      eventAD.parsed = true;
      return;
    }
    std::string_view trackName(trackIt->value.GetString());
    std::string_view trackTarget(
        NEJSON::ReadOptionalString(eventData, v2 ? NoodleExtensions::Constants::V2_TARGET.data()
                                                 : NoodleExtensions::Constants::TARGET.data())
            .value_or("Root"));
    TrackW track = beatmapAD.getTrack(trackName);
    NELogger::Logger.debug("Assigning player to track {} at {}, with target {}", trackName.data(), track.GetName(),
                           trackTarget.data());
    eventAD.playerTrackEventData.emplace(track, trackTarget);

    NECaches::hasPlayerTransfrom = true;
  }

  eventAD.parsed = true;
}

void LoadNoodleEvents(CustomJSONData::CustomBeatmapData* beatmap) {
  auto& beatmapAD = TracksAD::getBeatmapAD(beatmap->customData);

  auto v2 = beatmap->v2orEarlier;

  if (!beatmapAD.valid) {
    TracksAD::readBeatmapDataAD(beatmap);
  }

  // Parse events
  for (auto const& customEventData : beatmap->customEventDatas) {
    LoadNoodleEvent(beatmapAD, customEventData, v2);
  }
}

MAKE_HOOK_MATCH(BeatmapDataTransformHelper_CreateTransformedBeatmapData,
                &BeatmapDataTransformHelper::CreateTransformedBeatmapData, IReadonlyBeatmapData*,
                ::GlobalNamespace::IReadonlyBeatmapData* beatmapData, ::GlobalNamespace::BeatmapLevel* beatmapLevel,
                ::GlobalNamespace::GameplayModifiers* gameplayModifiers, bool leftHanded,
                ::GlobalNamespace::EnvironmentEffectsFilterPreset environmentEffectsFilterPreset,
                ::GlobalNamespace::EnvironmentIntensityReductionOptions* environmentIntensityReductionOptions,
                ::ByRef<::BeatSaber::Settings::Settings> settings) {
  auto result = BeatmapDataTransformHelper_CreateTransformedBeatmapData(beatmapData, beatmapLevel, gameplayModifiers,
                                                                        leftHanded, environmentEffectsFilterPreset,
                                                                        environmentIntensityReductionOptions, settings);

  if (!Hooks::isNoodleHookEnabled()) return result;

  if (auto customBeatmap = il2cpp_utils::try_cast<CustomJSONData::CustomBeatmapData>(result)) {
    auto& beatmapAD = TracksAD::getBeatmapAD(customBeatmap.value()->customData);

    if (!beatmapAD.valid) {
      TracksAD::readBeatmapDataAD(*customBeatmap);
    }

    LoadNoodleObjects(*customBeatmap);
    LoadNoodleEvents(*customBeatmap);

    NECaches::hasLocalSpaceTrail = false;
    auto customData = customBeatmap.value()->customData;
    if (customData != nullptr) {
      if (customData->value.has_value()) {
        auto localSpaceTrail =
            NEJSON::ReadOptionalBool(customData->value.value(), NoodleExtensions::Constants::TRAIL_LOCAL_SPACE);
        if (localSpaceTrail.has_value()) {
          NECaches::hasLocalSpaceTrail = localSpaceTrail.value();
        }
      }
    }
  }

  return result;
}

void HandleFakeObjects(CustomJSONData::v3::CustomBeatmapSaveData* beatmap) {
  if (FakeNoteHelper::EnsureV3FakeObjectsInjected(beatmap)) {
    NELogger::Logger.info("V3 fake-object parser callback completed");
  }
}

void InstallBeatmapDataTransformHelperHooks() {
  // Modloader::requireMod("CustomJSONData");
  CustomJSONData::v3::Parser::ParsedEvent += HandleFakeObjects;
  // SongCore::API::Loading::GetSongsLoadedEvent() += HandleFakeObjects;
  //  RuntimeSongLoader::API::AddBeatmapDataBasicInfoLoadedEvent(HandleFakeObjects);

  INSTALL_HOOK(NELogger::Logger, BeatmapDataTransformHelper_CreateTransformedBeatmapData);
}

NEInstallHooks(InstallBeatmapDataTransformHelperHooks);
