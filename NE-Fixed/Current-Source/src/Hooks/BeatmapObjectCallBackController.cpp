#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/CallbacksInTime.hpp"
#include "GlobalNamespace/BeatmapLineData.hpp"
#include "GlobalNamespace/BeatmapObjectData.hpp"
#include "GlobalNamespace/SortedList_1.hpp"
#include "GlobalNamespace/SortedList_2.hpp"
#include "GlobalNamespace/BeatmapCallbacksUpdater.hpp"
#include "GlobalNamespace/VariableMovementDataProvider.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/Action.hpp"

#include "custom-json-data/shared/CustomBeatmapData.h"
#include "Animation/Events.h"
#include "AssociatedData.h"
#include "tracks/shared/TimeSourceHelper.h"
#include "NEHooks.h"
#include "NELogger.h"
#include "SharedUpdate.h"
#include "NECaches.h"
#include "SpawnDataHelper.h"
#include "Constants.hpp"
#include "NEJSON.h"

#include "Zenject/DiContainer.hpp"

using namespace GlobalNamespace;

BeatmapCallbacksController* controller;
static GlobalNamespace::IReadonlyBeatmapData* beatmapData;

static BeatmapObjectSpawnController::InitData* initData;
static GlobalNamespace::BeatmapObjectSpawnMovementData* movementData;

static float GetSpawnAheadTime(BeatmapObjectSpawnController::InitData* initData,
                               BeatmapObjectSpawnMovementData* movementData, std::optional<float> inputNjs,
                               std::optional<float> inputOffset) {
  float const moveDuration = GlobalNamespace::VariableMovementDataProvider::kMoveDuration;
  return moveDuration + (SpawnDataHelper::GetJumpDuration(inputNjs, inputOffset) * 0.5f);
}

inline float ObjectSortGetTime(BeatmapDataItem* n) {
  static auto* customObstacleDataClass = classof(CustomJSONData::CustomObstacleData*);
  static auto* customNoteDataClass = classof(CustomJSONData::CustomNoteData*);
  static auto* customSliderDataClass = classof(CustomJSONData::CustomSliderData*);

  float* aheadTime;
  CustomJSONData::JSONWrapper* customDataWrapper;

  if (il2cpp_functions::class_is_assignable_from(customObstacleDataClass, n->klass)) {
    auto* obstacle = reinterpret_cast<CustomJSONData::CustomObstacleData*>(n);
    aheadTime = &obstacle->aheadTimeNoodle;
    customDataWrapper = obstacle->customData;
  } else if (il2cpp_functions::class_is_assignable_from(customNoteDataClass, n->klass)) {
    auto* note = reinterpret_cast<CustomJSONData::CustomNoteData*>(n);
    aheadTime = &note->aheadTimeNoodle;
    customDataWrapper = note->customData;
  } else if (il2cpp_functions::class_is_assignable_from(customSliderDataClass, n->klass)) {
    auto* note = reinterpret_cast<CustomJSONData::CustomSliderData*>(n);
    aheadTime = &note->aheadTimeNoodle;
    customDataWrapper = note->customData;
  } else {
    return n->time;
  }

  if (!customDataWrapper) return n->time;

  auto const& ad = getAD(customDataWrapper);

  auto njs = ad.objectData.noteJumpMovementSpeed;
  auto spawnOffset = ad.objectData.noteJumpStartBeatOffset;
  if ((!njs.has_value() || !spawnOffset.has_value()) &&
      customDataWrapper->value.has_value()) {
    auto const& customData = customDataWrapper->value.value().get();
    if (!njs.has_value()) {
      njs = NEJSON::ReadOptionalFloat(
          customData, NoodleExtensions::Constants::NOTE_JUMP_SPEED);
      if (!njs.has_value()) {
        njs = NEJSON::ReadOptionalFloat(
            customData, NoodleExtensions::Constants::V2_NOTE_JUMP_SPEED);
      }
    }
    if (!spawnOffset.has_value()) {
      spawnOffset = NEJSON::ReadOptionalFloat(
          customData, NoodleExtensions::Constants::NOTE_SPAWN_OFFSET);
      if (!spawnOffset.has_value()) {
        spawnOffset = NEJSON::ReadOptionalFloat(
            customData, NoodleExtensions::Constants::V2_NOTE_SPAWN_OFFSET);
      }
    }
  }

  *aheadTime = GetSpawnAheadTime(initData, movementData, njs, spawnOffset);

  return n->time - *aheadTime;
}

constexpr bool ObjectTimeCompare(BeatmapDataItem* a, BeatmapDataItem* b) {
  return ObjectSortGetTime(a) < ObjectSortGetTime(b);
}

System::Collections::Generic::LinkedList_1<BeatmapDataItem*>*
SortAndOrderList(CustomJSONData::CustomBeatmapData* beatmapData) {
  initData = NECaches::GameplayCoreContainer->Resolve<BeatmapObjectSpawnController::InitData*>();
  movementData = GlobalNamespace::BeatmapObjectSpawnMovementData::New_ctor();
  movementData->Init(initData->noteLinesCount, NECaches::JumpOffsetYProvider.ptr(), NEVector::Vector3::right());

  auto items = beatmapData->GetAllBeatmapItemsCpp();

  std::stable_sort(items.begin(), items.end(), ObjectTimeCompare);

  initData = nullptr;
  movementData = nullptr;

  auto newList = SafePtr(System::Collections::Generic::LinkedList_1<BeatmapDataItem*>::New_ctor());
  auto newListPtr = static_cast<System::Collections::Generic::LinkedList_1<BeatmapDataItem*>*>(newList);
  if (items.empty()) return newListPtr;

  for (auto const& o : items) {
    newList->AddLast(o);
  }

  return newListPtr;
}

MAKE_HOOK_MATCH(BeatmapCallbacksUpdater_LateUpdate, &BeatmapCallbacksUpdater::LateUpdate, void,
                BeatmapCallbacksUpdater* self) {
  auto selfController = self->_beatmapCallbacksController;

  // Reset to avoid overriding non NE maps
  //    if ((controller || beatmapData) && (controller != selfController || selfController->beatmapData != beatmapData))
  //    {
  //        CustomJSONData::CustomEventCallbacks::firstNode.emplace(nullptr);
  //    }

  if (!Hooks::isNoodleHookEnabled()) {
    controller = nullptr;
    beatmapData = nullptr;
    CustomJSONData::CustomEventCallbacks::firstNode.emplace(nullptr);
    return BeatmapCallbacksUpdater_LateUpdate(self);
  }

  auto beatmapOpt = il2cpp_utils::try_cast<CustomJSONData::CustomBeatmapData>(selfController->_beatmapData);
  if (beatmapOpt && (controller != selfController || selfController->_beatmapData != beatmapData)) {

    CJDLogger::Logger.fmtLog<Paper::LogLevel::INF>("Using noodle sorted node");
    controller = selfController;
    beatmapData = selfController->_beatmapData;

    auto items = SortAndOrderList(beatmapOpt.value());

    auto first = items->get_First();
    CustomJSONData::CustomEventCallbacks::firstNode.emplace(first);
  }

  return BeatmapCallbacksUpdater_LateUpdate(self);
}

void InstallBeatmapObjectCallbackControllerHooks() {
  INSTALL_HOOK(NELogger::Logger, BeatmapCallbacksUpdater_LateUpdate);
}

NEInstallHooks(InstallBeatmapObjectCallbackControllerHooks);
