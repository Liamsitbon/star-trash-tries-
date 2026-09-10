// SPDX-License-Identifier: MIT
// Adapted from the scene-scoped compatibility layer in ../Current-Source.
// No NE-private headers, state, type registrations or binary patches.
#include "Addon.hpp"
#include "NoteEffectPolicy.hpp"
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
#include <exception>

namespace NEFixed {
namespace {
using namespace GlobalNamespace;
rapidjson::Value const* Json(CustomJSONData::JSONWrapper* wrapper) {
  return wrapper && wrapper->value ? &wrapper->value->get() : nullptr;
}

template<class TPrefab, class TInstaller>
void KeepGamePrefab(Zenject::DiContainer* container, char const* contract) {
  auto registration = Lapiz::Objects::Registration<TPrefab, TInstaller>::New_ctor(
    contract, [](TPrefab prefab) { return prefab; }, 1000, false);
  registration->RegisterRedecorator(container);
}

void RegisterForDifficulty(Zenject::DiContainer* container) {
  if (!container || !ShouldApply(GetBaseStatus(), Enabled(), DefaultNotes(), true)) {
    SetLastScene("Add-on disabled; original behavior unchanged");
    return;
  }
  auto* setup = container->TryResolve<GameplayCoreSceneSetupData*>();
  auto* data = setup ? setup->get_transformedBeatmapData() : container->TryResolve<IReadonlyBeatmapData*>();
  auto* custom = data ? il2cpp_utils::try_cast<CustomJSONData::CustomBeatmapData>(data).value_or(nullptr) : nullptr;
  if (!custom) {
    SetLastScene("Ordinary beatmap; cosmetic notes unchanged");
    return;
  }
  NoteEffects::Policy policy;
  if (auto* json = Json(custom->customData)) policy.AddMapCustomData(*json);
  for (auto* object : custom->beatmapObjectDatas) {
    auto* note = object ? il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(object).value_or(nullptr) : nullptr;
    if (auto* json = note ? Json(note->customData) : nullptr) policy.AddNote(*json);
  }
  for (auto* event : custom->customEventDatas)
    if (auto* json = event ? Json(event->customData) : nullptr) policy.AddEvent(event->type, *json);

  if (!policy.RequiresDefaultNotes()) {
    SetLastScene("No note geometry/visibility effects; cosmetic notes unchanged");
    Logger.info("Scene note policy: no note effects; cosmetic selection retained");
    return;
  }
  // Lapiz 0.2.23 RegisterRedecorator binds into ancestor[0]. Check that exact
  // contract before allocating/binding. Registrations die with this scene.
  auto ancestors = container->get_AncestorContainers();
  if (!ancestors || ancestors.size() == 0 || !ancestors[0]) {
    SetLastScene("No gameplay parent container; compatibility not applied");
    Logger.warn("Note effects detected but no gameplay parent container; original NE remains active");
    return;
  }
  // Identity + priority 1000 precedes CustomModels 1.2.1's priority 300.
  // chain=false excludes cosmetic replacements only at prefab installation.
  // Vivify's authored note assignments run later; custom sabers are untouched.
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
  SetLastScene("Note effects: default gameplay prefabs; map-owned Vivify notes retained");
  Logger.info("Scene note policy applied: default notes/bombs/chains/mirrors/debris; saved cosmetics and sabers unchanged");
}
}  // namespace

void InstallNoteCompatibility() {
  Lapiz::Zenject::Zenjector::Get(Info())->Install(Lapiz::Zenject::Location::Player,
    [](Zenject::DiContainer* container) {
      try { RegisterForDifficulty(container); }
      catch (std::exception const& e) {
        SetLastScene("Compatibility callback failed; inspect NEFixed logcat");
        Logger.error("Scene compatibility callback failed: {}", e.what());
      }
    });
}
}  // namespace NEFixed
