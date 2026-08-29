#include "Animation/NoodleMovementDataProvider.hpp"
#include "NECaches.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/SliderController.hpp"
#include "GlobalNamespace/SliderMovement.hpp"
#include "GlobalNamespace/SliderData.hpp"
#include "GlobalNamespace/SliderSpawnData.hpp"

#include "NEHooks.h"
#include "NEObjectPool.hpp"

using namespace GlobalNamespace;

// Note and Obstacle movement data provider replacement hooks are consolidated
// directly in NoteController.cpp and ObstacleController.cpp to prevent duplicate
// hook collisions on Quest.

MAKE_HOOK_MATCH(ReplaceSliderMovement, &SliderController::Init, void,
                SliderController* self, SliderController::LengthType lengthType,
                SliderData* sliderData, ByRef<SliderSpawnData> sliderSpawnData,
                float noteUniformScale, float randomValue) {
  if (!Hooks::isNoodleHookEnabled())
    return ReplaceSliderMovement(self, lengthType, sliderData, sliderSpawnData, noteUniformScale, randomValue);

  if (!sliderData || !NECaches::noodleMovementDataProviderPool) {
    return ReplaceSliderMovement(self, lengthType, sliderData, sliderSpawnData, noteUniformScale, randomValue);
  }

  auto provider = NECaches::noodleMovementDataProviderPool->get(sliderData);

  auto IProvider = reinterpret_cast<IVariableMovementDataProvider*>(provider.ptr());
  self->_variableMovementDataProvider = IProvider;
  if (self->_sliderMovement) self->_sliderMovement->_variableMovementDataProvider = IProvider;

  ReplaceSliderMovement(self, lengthType, sliderData, sliderSpawnData, noteUniformScale, randomValue);
}

void InstallReplaceMovementDataProviderHooks() {
  INSTALL_HOOK(NELogger::Logger, ReplaceSliderMovement);
}

NEInstallHooks(InstallReplaceMovementDataProviderHooks);
