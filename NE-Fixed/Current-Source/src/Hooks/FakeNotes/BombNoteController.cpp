#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/CuttableBySaber.hpp"

#include "FakeNoteHelper.h"
#include "NEHooks.h"
#include "custom-json-data/shared/CustomBeatmapData.h"

using namespace GlobalNamespace;

MAKE_HOOK_MATCH(BombNoteController_Init, &BombNoteController::Init, void, BombNoteController* self, NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData) {
  BombNoteController_Init(self, noteData, noteSpawnData);
  if (!Hooks::isNoodleHookEnabled()) return;

  if (self->_cuttableBySaber) self->_cuttableBySaber->set_canBeCut(FakeNoteHelper::GetCuttable(noteData));
}

void InstallBombNoteControllerHooks() {
  INSTALL_HOOK(NELogger::Logger, BombNoteController_Init);
}
NEInstallHooks(InstallBombNoteControllerHooks);
