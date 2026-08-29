#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/MissionLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/EnvironmentsListModel.hpp"

#include "Animation/ParentObject.h"
#include "NEHooks.h"
#include "NECaches.h"
#include "SceneTransitionHelper.hpp"

using namespace GlobalNamespace;
using namespace TrackParenting;
using namespace CustomJSONData;
using namespace NoodleExtensions;

MAKE_HOOK_MATCH(
    MissionLevelScenesTransitionSetupDataSO_Init,
    static_cast<void (::GlobalNamespace::MissionLevelScenesTransitionSetupDataSO::*)(
        ::StringW, ByRef<::GlobalNamespace::BeatmapKey>, ::GlobalNamespace::BeatmapLevel*,
        ::ArrayW<::GlobalNamespace::MissionObjective*, ::Array<::GlobalNamespace::MissionObjective*>*>,
        ::GlobalNamespace::ColorScheme*, ::GlobalNamespace::GameplayModifiers*,
        ::GlobalNamespace::PlayerSpecificSettings*, ::GlobalNamespace::EnvironmentsListModel*,
        ::GlobalNamespace::BeatmapLevelsModel*, ::GlobalNamespace::AudioClipAsyncLoader*,
        ::GlobalNamespace::SettingsManager*, ::GlobalNamespace::BeatmapDataLoader*, ::StringW)>(
        &::GlobalNamespace::MissionLevelScenesTransitionSetupDataSO::Init),
    void, MissionLevelScenesTransitionSetupDataSO* self, ::StringW missionId,
    ByRef<::GlobalNamespace::BeatmapKey> beatmapKey, ::GlobalNamespace::BeatmapLevel* beatmapLevel,
    ::ArrayW<::GlobalNamespace::MissionObjective*, ::Array<::GlobalNamespace::MissionObjective*>*> missionObjectives,
    ::GlobalNamespace::ColorScheme* overrideColorScheme, ::GlobalNamespace::GameplayModifiers* gameplayModifiers,
    ::GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings,
    ::GlobalNamespace::EnvironmentsListModel* environmentsListModel,
    ::GlobalNamespace::BeatmapLevelsModel* beatmapLevelsModel,
    ::GlobalNamespace::AudioClipAsyncLoader* audioClipAsyncLoader,
    ::GlobalNamespace::SettingsManager* SettingsManager,
    ::GlobalNamespace::BeatmapDataLoader* beatmapDataLoader, ::StringW backButtonText) {
  auto customBeatmapLevel = il2cpp_utils::try_cast<SongCore::SongLoader::CustomBeatmapLevel>(beatmapLevel);
  if (!customBeatmapLevel) {
    MissionLevelScenesTransitionSetupDataSO_Init(self, missionId, beatmapKey, beatmapLevel, missionObjectives,
                                                 overrideColorScheme, gameplayModifiers, playerSpecificSettings,
                                                 environmentsListModel, beatmapLevelsModel, audioClipAsyncLoader,
                                                 SettingsManager, beatmapDataLoader, backButtonText);
    return;
  }

  auto environmentInfoBySerializedNameSafe = environmentsListModel->GetEnvironmentInfoBySerializedNameSafe(
      beatmapLevel->GetEnvironmentName(beatmapKey->beatmapCharacteristic, beatmapKey->difficulty)._environmentName);

  // Environment override is handled by Chroma - Noodle Extensions sets up the hooks
  // The actual environment override logic is Chroma's responsibility
  SceneTransitionHelper::Patch(customBeatmapLevel.value(), beatmapKey.heldRef, environmentInfoBySerializedNameSafe, playerSpecificSettings);

  MissionLevelScenesTransitionSetupDataSO_Init(self, missionId, beatmapKey, beatmapLevel, missionObjectives,
                                               overrideColorScheme, gameplayModifiers, playerSpecificSettings,
                                               environmentsListModel, beatmapLevelsModel, audioClipAsyncLoader,
                                               SettingsManager, beatmapDataLoader, backButtonText);
}

void InstallMissionLevelScenesTransitionSetupDataSOHooks() {
  INSTALL_HOOK(NELogger::Logger, MissionLevelScenesTransitionSetupDataSO_Init);
}
NEInstallHooks(InstallMissionLevelScenesTransitionSetupDataSOHooks);