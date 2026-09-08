#include "NoteEffectPolicy.hpp"
#include "NEConfig.h"
#include "NELogger.h"

#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "GlobalNamespace/FakeMirrorObjectsInstaller.hpp"
#include "GlobalNamespace/MirroredGameNoteController.hpp"
#include "GlobalNamespace/MirroredBombNoteController.hpp"
#include "GlobalNamespace/NoteDebris.hpp"
#include "GlobalNamespace/NoteDebrisPoolInstaller.hpp"
#include "Zenject/FromBinderGeneric_1.hpp"
#include "Zenject/ScopeConcreteIdArgConditionCopyNonLazyBinder.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "lapiz/shared/objects/Beatmap.hpp"
#include "lapiz/shared/zenject/Zenjector.hpp"

namespace {
using namespace GlobalNamespace;

rapidjson::Value const* Json(CustomJSONData::JSONWrapper* wrapper) {
  return wrapper && wrapper->value ? &wrapper->value->get() : nullptr;
}

template<class TPrefab, class TInstaller>
void KeepGamePrefab(Zenject::DiContainer* container, char const* contract) {
  // CustomModels 1.2.1 registers at priority 300. An identity redecorator
  // before cosmetic replacements preserves Beat Saber's cutout bindings.
  // chain=false stops note cosmetics for this scene, not map-owned Vivify
  // assignments. Do not mutate saved settings or any shared prefab asset.
  auto registration = Lapiz::Objects::Registration<TPrefab, TInstaller>::New_ctor(
      contract, [](TPrefab prefab) { return prefab; }, 1000, false);
  registration->RegisterRedecorator(container);
}

void RegisterForDifficulty(Zenject::DiContainer* container) {
  if (!container || !getNEConfig().defaultNotesForNoteEffects.GetValue()) return;
  auto* setup = container->TryResolve<GameplayCoreSceneSetupData*>();
  auto* data = setup ? setup->get_transformedBeatmapData()
                     : container->TryResolve<IReadonlyBeatmapData*>();
  auto* custom = data ? il2cpp_utils::try_cast<CustomJSONData::CustomBeatmapData>(data).value_or(nullptr)
                      : nullptr;
  if (!custom) return;

  NoodleExtensions::NoteEffects::Policy policy;
  for (auto* object : custom->beatmapObjectDatas) {
    auto* note = object ? il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(object).value_or(nullptr)
                        : nullptr;
    if (auto* json = note ? Json(note->customData) : nullptr) policy.AddNote(*json);
  }
  for (auto* event : custom->customEventDatas) {
    if (auto* json = event ? Json(event->customData) : nullptr) policy.AddEvent(event->type, *json);
  }
  if (!policy.RequiresDefaultNotes()) return;
  auto ancestors = container->get_AncestorContainers();
  if (!ancestors || ancestors.size() == 0) {
    NELogger::Logger.warn("Default-note compatibility could not find the gameplay parent container");
    return;
  }

  for (auto* field : {"_normalBasicNotePrefab", "_proModeNotePrefab", "_burstSliderHeadNotePrefab"})
    KeepGamePrefab<GameNoteController*, BeatmapObjectsInstaller*>(container, field);
  KeepGamePrefab<BurstSliderGameNoteController*, BeatmapObjectsInstaller*>(container, "_burstSliderNotePrefab");
  KeepGamePrefab<BombNoteController*, BeatmapObjectsInstaller*>(container, "_bombNotePrefab");
  for (auto* field : {"_mirroredGameNoteControllerPrefab", "_mirroredBurstSliderHeadGameNoteControllerPrefab",
                     "_mirroredBurstSliderGameNoteControllerPrefab"})
    KeepGamePrefab<MirroredGameNoteController*, FakeMirrorObjectsInstaller*>(container, field);
  KeepGamePrefab<MirroredBombNoteController*, FakeMirrorObjectsInstaller*>(container, "_mirroredBombNoteControllerPrefab");
  for (auto* field : {"_normalNoteDebrisHDPrefab", "_burstSliderHeadNoteDebrisHDPrefab",
                     "_burstSliderElementNoteHDPrefab", "_normalNoteDebrisLWPrefab",
                     "_burstSliderHeadNoteDebrisLWPrefab", "_burstSliderElementNoteLWPrefab"})
    KeepGamePrefab<NoteDebris*, NoteDebrisPoolInstaller*>(container, field);

  NELogger::Logger.info("Note-effect map: using default note prefabs for this gameplay scene; custom sabers and saved cosmetic selection unchanged");
}
}  // namespace

void InstallCustomNoteCompatibility() {
  static modloader::ModInfo const compatibilityMod{MOD_ID, VERSION, 0};
  Lapiz::Zenject::Zenjector::Get(compatibilityMod)->Install(
      Lapiz::Zenject::Location::Player, RegisterForDifficulty);
}
