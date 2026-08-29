#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BasicBeatmapObjectManager.hpp"
#include "GlobalNamespace/ObstacleController.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteMovement.hpp"
#include "GlobalNamespace/NoteFloorMovement.hpp"
#include "GlobalNamespace/NoteJump.hpp"
#include "GlobalNamespace/NoteWaiting.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"
#include "GlobalNamespace/SliderController.hpp"
#include "GlobalNamespace/SliderMovement.hpp"
#include "GlobalNamespace/IVariableMovementDataProvider.hpp"
#include "GlobalNamespace/VariableMovementDataProvider.hpp"

#include "System/Collections/Generic/List_1.hpp"

#include "Animation/NoodleMovementDataProvider.hpp"
#include "NECaches.h"
#include "NEHooks.h"
#include "NEObjectPool.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"

using namespace GlobalNamespace;
using namespace UnityEngine;

// added to by ObstacleController Init
SafePtr<System::Collections::Generic::List_1<UnityW<ObstacleController>>>& getActiveObstacles();

static SafePtr<NoodleExtensions::NoodleMovementDataProvider>
CaptureNoodleProvider(IVariableMovementDataProvider* provider) {
  SafePtr<NoodleExtensions::NoodleMovementDataProvider> result;
  auto* object = reinterpret_cast<Il2CppObject*>(provider);
  if (object != nullptr && object->klass == classof(NoodleExtensions::NoodleMovementDataProvider*)) {
    result.emplace(reinterpret_cast<NoodleExtensions::NoodleMovementDataProvider*>(provider));
  }
  return result;
}

static void ReturnNoodleProvider(SafePtr<NoodleExtensions::NoodleMovementDataProvider> provider) {
  if (!provider || !NECaches::noodleMovementDataProviderPool) return;
  NECaches::noodleMovementDataProviderPool->put(provider);
}

MAKE_HOOK_MATCH(BasicBeatmapObjectManager_Init, &BasicBeatmapObjectManager::Init, void, BasicBeatmapObjectManager* self,
                ::GlobalNamespace::BasicBeatmapObjectManager_InitData* initData, ::System::Random* random,
                ::GlobalNamespace::VariableMovementDataProvider* variableMovementDataProvider,
                ::GlobalNamespace::GameNoteController_Pool* basicGameNotePool,
                ::GlobalNamespace::GameNoteController_Pool* burstSliderHeadGameNotePool,
                ::GlobalNamespace::BurstSliderGameNoteController_Pool* burstSliderGameNotePool,
                ::GlobalNamespace::BombNoteController_Pool* bombNotePool,
                ::GlobalNamespace::ObstacleController_Pool* obstaclePool,
                ::GlobalNamespace::SliderController_Pool* sliderPools) {
  BasicBeatmapObjectManager_Init(self, initData, random, variableMovementDataProvider, basicGameNotePool, burstSliderHeadGameNotePool,
                                 burstSliderGameNotePool, bombNotePool, obstaclePool, sliderPools);
  if (!Hooks::isNoodleHookEnabled()) return;

  // This makes sure that the list of obstacles is cleared every time a new BasicBeatmapObjectManager is initialized (on new level load or restart)
  // Without this, dead obstaclecontrollers would pile up in the list and cause nullref exceptions later on
  getActiveObstacles().emplace(ListW<UnityW<ObstacleController>>());
}

MAKE_HOOK_MATCH(BasicBeatmapObjectManager_get_activeObstacleControllers,
                &BasicBeatmapObjectManager::get_activeObstacleControllers,
                System::Collections::Generic::List_1<UnityW<GlobalNamespace::ObstacleController>>*,
                BasicBeatmapObjectManager* self) {
  if (!Hooks::isNoodleHookEnabled()) return BasicBeatmapObjectManager_get_activeObstacleControllers(self);

  auto& activeObstacles = getActiveObstacles();
  return activeObstacles ? activeObstacles.ptr() : BasicBeatmapObjectManager_get_activeObstacleControllers(self);
}

MAKE_HOOK_MATCH(BasicBeatmapObjectManager_DespawnInternal_Obstacle,
                static_cast<void (GlobalNamespace::BasicBeatmapObjectManager::*)(GlobalNamespace::ObstacleController*)>(
                    &GlobalNamespace::BasicBeatmapObjectManager::DespawnInternal),
                void, BasicBeatmapObjectManager* self, ObstacleController* obstacleController) {
  if (!Hooks::isNoodleHookEnabled()) {
    BasicBeatmapObjectManager_DespawnInternal_Obstacle(self, obstacleController);
    return;
  }

  auto provider = CaptureNoodleProvider(obstacleController ? obstacleController->_variableMovementDataProvider : nullptr);
  BasicBeatmapObjectManager_DespawnInternal_Obstacle(self, obstacleController);
  if (obstacleController && provider) {
    obstacleController->_variableMovementDataProvider =
        reinterpret_cast<IVariableMovementDataProvider*>(self->_variableMovementDataProvider);
    ReturnNoodleProvider(provider);
  }
  auto& activeObstacles = getActiveObstacles();
  if (activeObstacles && obstacleController) activeObstacles->Remove(obstacleController);
}

MAKE_HOOK_MATCH(BasicBeatmapObjectManager_DespawnInternal_Note,
                static_cast<void (GlobalNamespace::BasicBeatmapObjectManager::*)(GlobalNamespace::NoteController*)>(
                    &GlobalNamespace::BasicBeatmapObjectManager::DespawnInternal),
                void, BasicBeatmapObjectManager* self, NoteController* noteController) {
  if (!Hooks::isNoodleHookEnabled()) {
    BasicBeatmapObjectManager_DespawnInternal_Note(self, noteController);
    return;
  }

  auto* noteMovement = noteController ? noteController->_noteMovement.unsafePtr() : nullptr;
  auto provider = CaptureNoodleProvider(noteMovement ? noteMovement->_variableMovementDataProvider : nullptr);
  BasicBeatmapObjectManager_DespawnInternal_Note(self, noteController);
  if (noteController && noteMovement && provider) {
    auto* fallback = reinterpret_cast<IVariableMovementDataProvider*>(self->_variableMovementDataProvider);
    noteMovement->_variableMovementDataProvider = fallback;
    if (noteMovement->_floorMovement) noteMovement->_floorMovement->_variableMovementDataProvider = fallback;
    if (noteMovement->_jump) noteMovement->_jump->_variableMovementDataProvider = fallback;
    if (noteMovement->_waiting) noteMovement->_waiting->_variableMovementDataProvider = fallback;
    if (il2cpp_utils::AssignableFrom<BurstSliderGameNoteController*>(noteController->klass)) {
      reinterpret_cast<BurstSliderGameNoteController*>(noteController)->_variableMovementDataProvider = fallback;
    }
    ReturnNoodleProvider(provider);
  }
}

MAKE_HOOK_MATCH(BasicBeatmapObjectManager_DespawnInternal_Slider,
                static_cast<void (GlobalNamespace::BasicBeatmapObjectManager::*)(GlobalNamespace::SliderController*)>(
                    &GlobalNamespace::BasicBeatmapObjectManager::DespawnInternal),
                void, BasicBeatmapObjectManager* self, SliderController* sliderController) {
  if (!Hooks::isNoodleHookEnabled()) {
    BasicBeatmapObjectManager_DespawnInternal_Slider(self, sliderController);
    return;
  }

  auto provider = CaptureNoodleProvider(sliderController ? sliderController->_variableMovementDataProvider : nullptr);
  BasicBeatmapObjectManager_DespawnInternal_Slider(self, sliderController);
  if (sliderController && provider) {
    auto* fallback = reinterpret_cast<IVariableMovementDataProvider*>(self->_variableMovementDataProvider);
    sliderController->_variableMovementDataProvider = fallback;
    if (sliderController->_sliderMovement) sliderController->_sliderMovement->_variableMovementDataProvider = fallback;
    ReturnNoodleProvider(provider);
  }
}

void InstallBasicBeatmapObjectManagerHooks() {
  INSTALL_HOOK(NELogger::Logger, BasicBeatmapObjectManager_Init);
  INSTALL_HOOK(NELogger::Logger, BasicBeatmapObjectManager_get_activeObstacleControllers);
  INSTALL_HOOK(NELogger::Logger, BasicBeatmapObjectManager_DespawnInternal_Obstacle);
  INSTALL_HOOK(NELogger::Logger, BasicBeatmapObjectManager_DespawnInternal_Note);
  INSTALL_HOOK(NELogger::Logger, BasicBeatmapObjectManager_DespawnInternal_Slider);
}

NEInstallHooks(InstallBasicBeatmapObjectManagerHooks);
