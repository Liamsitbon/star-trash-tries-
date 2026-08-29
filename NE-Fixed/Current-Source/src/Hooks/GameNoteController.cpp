#include "NELogger.h"
#include "VariableMovementHelper.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "GlobalNamespace/SaberTypeExtensions.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/NoteBasicCutInfoHelper.hpp"

#include "NEConfig.h"
#include "NEUtils.hpp"
#include "Animation/AnimationHelper.h"
#include "Animation/ParentObject.h"
#include "tracks/shared/TimeSourceHelper.h"
#include "AssociatedData.h"
#include "NEHooks.h"

using namespace GlobalNamespace;
using namespace UnityEngine;

MAKE_HOOK_MATCH(GameNoteController_HandleCut, &GameNoteController::HandleCut, void, GameNoteController* self,
                Saber* saber, Vector3 cutPoint, Quaternion orientation,
                Vector3 cutDirVec, bool allowBadCut) {
  if (!Hooks::isNoodleHookEnabled()) return GameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);

  auto customNoteData = il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(self->_noteData);
  if (customNoteData && customNoteData.value()->customData && customNoteData.value()->customData->value) {
    BeatmapObjectAssociatedData& ad = getAD(customNoteData.value()->customData);
    
    bool disableBadCutDirection = ad.objectData.disableBadCutDirection;
    bool disableBadCutSaberType = ad.objectData.disableBadCutSaberType;
    bool disableBadCutSpeed = ad.objectData.disableBadCutSpeed;

    if (disableBadCutDirection || disableBadCutSaberType || disableBadCutSpeed) {
      allowBadCut = true;
    }
  }

  GameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}

// postfix '_BadCutsModifier' because FakeNotes use that too
void InstallGameNoteControllerHooks_BadCutsModifier() {
  INSTALL_HOOK(NELogger::Logger, GameNoteController_HandleCut);
}

NEInstallHooks(InstallGameNoteControllerHooks_BadCutsModifier);
