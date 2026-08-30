#include <algorithm>

#include "Animation/Events.h"
#include "Animation/ParentObject.h"
#include "NECaches.h"
#include "NEConfig.h"
#include "NEHooks.h"
#include "NELogger.h"
#include "QuestInterop.hpp"
#include "Zenject/DiContainer.hpp"
#include "custom-json-data/shared/CJDLogger.h"
#include "songcore/shared/SongCore.hpp"

#include "scotland2/shared/loader.hpp"
#include "scotland2/shared/modloader.h"

float NECaches::noteJumpMovementSpeed;
float NECaches::noteJumpStartBeatOffset;
float NECaches::numberOfLines;
float NECaches::beatsPerMinute;
float NECaches::noteJumpValue;
GlobalNamespace::BeatmapObjectSpawnMovementData::NoteJumpValueType NECaches::noteJumpValueType;
bool NECaches::hasLocalSpaceTrail;
bool NECaches::hasPlayerTransfrom;
bool NECaches::LeftHandedMode;
bool NECaches::VivifyActive;
bool NECaches::NexoraActive;
bool NECaches::CinemaActive;
bool NECaches::SharedTracksRuntimeActive;
SafePtr<Zenject::DiContainer> NECaches::GameplayCoreContainer;
SafePtr<GlobalNamespace::IJumpOffsetYProvider> NECaches::JumpOffsetYProvider;
SafePtr<GlobalNamespace::VariableMovementDataProvider> NECaches::VariableMovementDataProvider;
SafePtr<GlobalNamespace::BeatmapObjectSpawnController::InitData> NECaches::InitData;
SafePtr<GlobalNamespace::BeatmapObjectSpawnController> NECaches::beatmapObjectSpawnController;
SafePtr<CustomJSONData::CustomBeatmapData> NECaches::customBeatmapData;
std::shared_ptr<NoodleExtensions::Pool::NoodleMovementDataProviderPool> NECaches::noodleMovementDataProviderPool;

void NECaches::ResetRuntimeState(char const* reason) {
  auto const noteEntries = noteCache.size();
  auto const obstacleEntries = obstacleCache.size();
  auto const sliderEntries = sliderCache.size();

  // Disable first so no hook can observe half-reset state.
  Hooks::setNoodleHookEnabled(false);

  ClearNoteCaches();
  ClearObstacleCaches();
  ClearSliderCaches();
  ParentController::OnDestroy();
  noodleMovementDataProviderPool.reset();

  GameplayCoreContainer = {};
  JumpOffsetYProvider = {};
  VariableMovementDataProvider = {};
  InitData = {};
  beatmapObjectSpawnController = {};
  customBeatmapData = {};

  noteJumpMovementSpeed = 0.0f;
  noteJumpStartBeatOffset = 0.0f;
  numberOfLines = 0.0f;
  beatsPerMinute = 0.0f;
  noteJumpValue = 0.0f;
  noteJumpValueType = GlobalNamespace::BeatmapObjectSpawnMovementData::NoteJumpValueType::JumpDuration;
  hasLocalSpaceTrail = false;
  hasPlayerTransfrom = false;
  LeftHandedMode = false;
  VivifyActive = false;
  NexoraActive = false;
  CinemaActive = false;
  SharedTracksRuntimeActive = false;

  if (getNEConfig().runtimeDiagnostics.GetValue()) {
    NELogger::Logger.info(
        "Runtime reset ({}): cleared {} note, {} obstacle, and {} slider cache entries",
        reason ? reason : "unknown", noteEntries, obstacleEntries, sliderEntries);
  }
}

void InstallAndRegisterAll() {
  static bool registered = false;
  if (registered) {
    NELogger::Logger.warn("InstallAndRegisterAll was called twice; second registration was ignored");
    return;
  }
  registered = true;

  auto cjdModInfo = CustomJSONData::modInfo.to_c();
  auto tracksModInfo = CModInfo{ .id = "Tracks" };

  modloader_require_mod(&cjdModInfo, CMatchType::MatchType_IdOnly);
  modloader_require_mod(&tracksModInfo, CMatchType::MatchType_IdOnly);
  // Chroma is compatible, but it is not required for Noodle animation maps.

  Hooks::InstallHooks();
  NEEvents::AddEventCallbacks();
  if (!SongCore::API::Capabilities::IsCapabilityRegistered(
          NoodleExtensions::U8_REQUIREMENTNAME)) {
    SongCore::API::Capabilities::RegisterCapability(
        NoodleExtensions::U8_REQUIREMENTNAME);
  }

  SongCore::API::LevelSelect::GetLevelWasSelectedEvent() +=
      [](SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
        // A previous NE+ME selection may have disabled the play button. Always clear that
        // state when moving to an official map or an incomplete selection.
        if (!event.isCustom || !event.customLevelDetails) {
          SongCore::API::PlayButton::EnablePlayButton(NoodleExtensions::U8_REQUIREMENTNAME);
          return;
        }

        auto const& requirements = event.customLevelDetails->difficultyDetails.requirements;
        auto const context = QuestModInterop::Inspect(requirements);
        bool meRequirement = std::any_of(requirements.begin(), requirements.end(),
                                         [](auto const& s) { return s == NoodleExtensions::U8_ME_REQUIREMENTNAME; });
        bool neRequirement = std::any_of(requirements.begin(), requirements.end(),
                                         [](auto const& s) { return s == NoodleExtensions::U8_REQUIREMENTNAME; });
        bool vivifyRequirement = context.required.vivify;
        bool conflict = meRequirement && neRequirement;
        bool blockConflict = conflict && getNEConfig().disableOnMappingExtensionsConflict.GetValue();

        if (getNEConfig().runtimeDiagnostics.GetValue()) {
          NELogger::Logger.info(
              "Selected custom difficulty: NE={}, ME={}, Vivify={}, Nexora={}, Cinema={}, conflict={}, blocked={}; installed[C={} N={} NE={} V={}]",
              neRequirement, meRequirement, vivifyRequirement,
              context.required.nexora, context.required.cinema, conflict,
              blockConflict, context.installed.cinema, context.installed.nexora,
              context.installed.noodleExtensions, context.installed.vivify);
        }

        if (blockConflict) {
          SongCore::API::PlayButton::DisablePlayButton(
              NoodleExtensions::U8_REQUIREMENTNAME,
              "Map requires both Noodle Extensions and Mapping Extensions. This combination is not supported.");
        } else {
          SongCore::API::PlayButton::EnablePlayButton(NoodleExtensions::U8_REQUIREMENTNAME);
        }
      };

  NELogger::Logger.info("NoodleExtensions runtime registered successfully ({} hook groups)",
                        Hooks::RegisteredHookGroupCount());
}
