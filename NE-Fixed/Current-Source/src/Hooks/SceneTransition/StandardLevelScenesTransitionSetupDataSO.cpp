#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"

#include "custom-json-data/shared/CustomBeatmapSaveDatav3.h"

#include "Animation/ParentObject.h"
#include "NEHooks.h"
#include "NECaches.h"
#include "SceneTransitionHelper.hpp"

using namespace GlobalNamespace;
using namespace TrackParenting;
using namespace CustomJSONData;
using namespace NoodleExtensions;

MAKE_HOOK_MATCH(StandardLevelScenesTransitionSetupDataSO_Init,
                &StandardLevelScenesTransitionSetupDataSO::InitAndSetupScenes, void,
                StandardLevelScenesTransitionSetupDataSO* self,
                ::GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings, ::StringW backButtonText,
                bool startPaused) {
  auto customBeatmapLevel = il2cpp_utils::try_cast<SongCore::SongLoader::CustomBeatmapLevel>(self->get_beatmapLevel());
  if (!customBeatmapLevel) {
    StandardLevelScenesTransitionSetupDataSO_Init(self, playerSpecificSettings, backButtonText, startPaused);
    return;
  }

  // Environment override is handled by Chroma - Noodle Extensions sets up the hooks
  // The actual environment override logic is Chroma's responsibility
  SceneTransitionHelper::Patch(customBeatmapLevel.value(), self->beatmapKey, self->targetEnvironmentInfo,
                               playerSpecificSettings);

  StandardLevelScenesTransitionSetupDataSO_Init(self, playerSpecificSettings, backButtonText, startPaused);
}

void InstallStandardLevelScenesTransitionSetupDataSOHooks() {
  INSTALL_HOOK(NELogger::Logger, StandardLevelScenesTransitionSetupDataSO_Init);
}

NEInstallHooks(InstallStandardLevelScenesTransitionSetupDataSOHooks);