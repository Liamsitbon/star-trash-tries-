#include "NELogger.h"
#include "VariableMovementHelper.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/MirroredGameNoteController.hpp"
#include "GlobalNamespace/NoteFloorMovement.hpp"
#include "GlobalNamespace/NoteJump.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteMovement.hpp"
#include "GlobalNamespace/BaseNoteVisuals.hpp"
#include "GlobalNamespace/CutoutAnimateEffect.hpp"
#include "GlobalNamespace/CutoutEffect.hpp"
#include "GlobalNamespace/DisappearingArrowControllerBase_1.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/ConditionalMaterialSwitcher.hpp"
#include "GlobalNamespace/MaterialPropertyBlockController.hpp"
#include "GlobalNamespace/BoxCuttableBySaber.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"
#include "GlobalNamespace/NoteWaiting.hpp"
#include "Animation/NoodleMovementDataProvider.hpp"
#include "NEObjectPool.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/GameObject.hpp"

#include <string_view>
#include <unordered_set>

#include "NEConfig.h"
#include "NEUtils.hpp"
#include "Animation/AnimationHelper.h"
#include "Animation/ParentObject.h"
#include "tracks/shared/TimeSourceHelper.h"
#include "AssociatedData.h"
#include "NEHooks.h"
#include "NECaches.h"

#include "custom-json-data/shared/CustomBeatmapData.h"
#include "custom-json-data/shared/JsonUtils.h"

#include "sombrero/shared/linq_functional.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;
using namespace TrackParenting;

BeatmapObjectAssociatedData* noteUpdateAD = nullptr;
TracksAD::TracksVector noteTracks;

std::unordered_map<std::string, std::unordered_set<NoteController*>> linkedNotes;
std::unordered_map<NoteController*, std::unordered_set<NoteController*>*> linkedLinkedNotes;

static void SetGameNoteCuttable(GameNoteController* controller, bool enabled) {
  if (!controller) return;
  for (auto cuttable : controller->_bigCuttableBySaberList) {
    if (cuttable && cuttable->canBeCut != enabled) cuttable->set_canBeCut(enabled);
  }
  for (auto cuttable : controller->_smallCuttableBySaberList) {
    if (cuttable && cuttable->canBeCut != enabled) cuttable->set_canBeCut(enabled);
  }
}

CutoutEffect* NECaches::GetCutout(GlobalNamespace::NoteControllerBase* nc, NECaches::NoteCache& noteCache) {
  CutoutEffect*& cutoutEffect = noteCache.cutoutEffect;

  if (cutoutEffect) return cutoutEffect;

  if (nc == nullptr || nc->get_gameObject() == nullptr) return nullptr;
  noteCache.baseNoteVisuals = noteCache.baseNoteVisuals ?: nc->get_gameObject()->GetComponent<BaseNoteVisuals*>();
  if (noteCache.baseNoteVisuals == nullptr) return nullptr;
  CutoutAnimateEffect* cutoutAnimateEffect = noteCache.baseNoteVisuals->_cutoutAnimateEffect;
  if (cutoutAnimateEffect == nullptr) return nullptr;
  ArrayW<UnityW<CutoutEffect>> cuttoutEffects = cutoutAnimateEffect->_cuttoutEffects;
  for (CutoutEffect* effect : cuttoutEffects) {
    if (effect->get_name() != u"NoteArrow") {
      cutoutEffect = effect;
      break;
    }
  }

  return cutoutEffect;
}

GlobalNamespace::DisappearingArrowControllerBase_1<GlobalNamespace::GameNoteController*>*
NECaches::GetDisappearingArrowController(GlobalNamespace::GameNoteController* nc, NECaches::NoteCache& noteCache) {
  auto& disappearingArrowController = noteCache.disappearingArrowController;
  if (!disappearingArrowController && nc != nullptr && nc->get_gameObject() != nullptr) {
    disappearingArrowController =
        nc->get_gameObject()->GetComponent<DisappearingArrowControllerBase_1<GameNoteController*>*>();
  }

  return disappearingArrowController;
}

GlobalNamespace::DisappearingArrowControllerBase_1<GlobalNamespace::MirroredGameNoteController*>*
NECaches::GetDisappearingArrowController(GlobalNamespace::MirroredGameNoteController* nc,
                                         NECaches::NoteCache& noteCache) {
  auto& disappearingArrowController = noteCache.mirroredDisappearingArrowController;
  if (!disappearingArrowController && nc != nullptr && nc->get_gameObject() != nullptr) {
    disappearingArrowController =
        nc->get_gameObject()->GetComponent<DisappearingArrowControllerBase_1<MirroredGameNoteController*>*>();
  }

  return disappearingArrowController;
}

float noteTimeAdjust(float original, float jumpDuration) {
  if (noteTracks.empty()) return original;

  auto time = NoodleExtensions::getTimeProp(noteTracks);

  if (time) {
    return *time * jumpDuration;
  }

  return original;
}

void NECaches::ClearNoteCaches() {
  NECaches::noteCache.clear();
  noteUpdateAD = nullptr;
  noteTracks.clear();
  linkedNotes.clear();
  linkedLinkedNotes.clear();
}

MAKE_HOOK_MATCH(NoteController_Init, &NoteController::Init, void, NoteController* self,
                GlobalNamespace::NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData,
                float_t endRotation, float_t uniformScale, bool rotateTowardsPlayer, bool useRandomRotation) {

  NoodleExtensions::NoodleMovementDataProvider* noodleProvider = nullptr;
  if (Hooks::isNoodleHookEnabled() && noteData != nullptr && NECaches::noodleMovementDataProviderPool) {
    auto provider = NECaches::noodleMovementDataProviderPool->get(noteData);
    noodleProvider = provider.ptr();
    auto IProvider = reinterpret_cast<IVariableMovementDataProvider*>(provider.ptr());

    auto noteMovement = self->_noteMovement;
    if (noteMovement != nullptr) {
      auto noteFloorMovement = noteMovement->_floorMovement;
      auto noteJump = noteMovement->_jump;
      auto noteWaiting = noteMovement->_waiting;

      noteMovement->_variableMovementDataProvider = IProvider;
      if (noteFloorMovement != nullptr) noteFloorMovement->_variableMovementDataProvider = IProvider;
      if (noteJump != nullptr) noteJump->_variableMovementDataProvider = IProvider;
      if (noteWaiting != nullptr) noteWaiting->_variableMovementDataProvider = IProvider;
    }

    if (il2cpp_utils::AssignableFrom<BurstSliderGameNoteController*>(self->klass)) {
      auto burstSliderGameNoteController = reinterpret_cast<BurstSliderGameNoteController*>(self);
      burstSliderGameNoteController->_variableMovementDataProvider = IProvider;
    }
  }

  NoteController_Init(self, noteData, noteSpawnData, endRotation, uniformScale, rotateTowardsPlayer, useRandomRotation);

  // 1.40.8 can refresh the movement subcomponents during Init. Reassert the
  // same rooted per-object provider afterwards so long spawn offsets cannot
  // silently fall back to the difficulty's short default jump duration.
  if (noodleProvider != nullptr && self->_noteMovement != nullptr) {
    auto* IProvider = reinterpret_cast<IVariableMovementDataProvider*>(noodleProvider);
    auto* noteMovement = self->_noteMovement.unsafePtr();
    noteMovement->_variableMovementDataProvider = IProvider;
    if (noteMovement->_floorMovement != nullptr) {
      noteMovement->_floorMovement->_variableMovementDataProvider = IProvider;
    }
    if (noteMovement->_jump != nullptr) {
      noteMovement->_jump->_variableMovementDataProvider = IProvider;
    }
    if (noteMovement->_waiting != nullptr) {
      noteMovement->_waiting->_variableMovementDataProvider = IProvider;
    }
    if (il2cpp_utils::AssignableFrom<BurstSliderGameNoteController*>(self->klass)) {
      reinterpret_cast<BurstSliderGameNoteController*>(self)
          ->_variableMovementDataProvider = IProvider;
    }
  }

  if (!Hooks::isNoodleHookEnabled()) return;

  if (!noteData) return;

  static auto CustomKlass = classof(CustomJSONData::CustomNoteData*);
  static auto* gameNoteControllerClass = classof(GameNoteController*);

  // Beat Saber recycles controllers. Always restore real notes to a cuttable
  // baseline before applying an uninteractable fake-note override.
  if (il2cpp_functions::class_is_assignable_from(gameNoteControllerClass, self->klass)) {
    SetGameNoteCuttable(reinterpret_cast<GameNoteController*>(self), true);
  }

  if (!il2cpp_functions::class_is_assignable_from(CustomKlass, noteData->klass)) return;

  auto* customNoteData = reinterpret_cast<CustomJSONData::CustomNoteData*>(noteData);

  Transform* transform = self->get_transform();
  transform->set_localScale(NEVector::Vector3::one()); // This is a fix for animation due to notes being
  // recycled

  if (!customNoteData->customData) return;
  BeatmapObjectAssociatedData& ad = getAD(customNoteData->customData);

  if (!ad.parsed) return;

  if (il2cpp_functions::class_is_assignable_from(gameNoteControllerClass, self->klass)) {
    SetGameNoteCuttable(reinterpret_cast<GameNoteController*>(self), !ad.objectData.uninteractable.value_or(false));
  }

  auto link = ad.objectData.link;
  if (link) {
    auto& list = linkedNotes[*link];
    list.emplace(self);
    linkedLinkedNotes[self] = &list;
  }

  // TRANSPILERS SUCK!
  auto flipYSide = ad.flipY ? *ad.flipY : customNoteData->flipYSide;

  if (flipYSide > 0.0f) {
    self->_noteMovement->_jump->_yAvoidance = flipYSide * self->_noteMovement->_jump->_yAvoidanceUp;
  } else {
    self->_noteMovement->_jump->_yAvoidance = flipYSide * self->_noteMovement->_jump->_yAvoidanceDown;
  }

  auto& noteCache = NECaches::getNoteCache(self);

  // TODO: reimplement material switching
  ArrayW<ConditionalMaterialSwitcher*>& materialSwitchers = noteCache.conditionalMaterialSwitchers;
  if (!materialSwitchers) {
    materialSwitchers = self->GetComponentsInChildren<ConditionalMaterialSwitcher*>();
  }

  for (auto* materialSwitcher : materialSwitchers) {
    if (materialSwitcher == nullptr || materialSwitcher->_renderer == nullptr) continue;
    materialSwitcher->_renderer->set_sharedMaterial(materialSwitcher->_material0);
  }
  noteCache.dissolveEnabled = false;

  NoteJump* noteJump = self->_noteMovement->_jump;
  NoteFloorMovement* floorMovement = self->_noteMovement->_floorMovement;

  NEVector::Quaternion localRotation = NEVector::Quaternion::identity();
  if (ad.objectData.rotation || ad.objectData.localRotation) {
    if (ad.objectData.localRotation) {
      localRotation = *ad.objectData.localRotation;
    }

    if (ad.objectData.rotation) {
      NEVector::Quaternion worldRotationQuatnerion = *ad.objectData.rotation;

      NEVector::Quaternion inverseWorldRotation = NEVector::Quaternion::Inverse(worldRotationQuatnerion);
      noteJump->_worldRotation = worldRotationQuatnerion;
      noteJump->_inverseWorldRotation = inverseWorldRotation;
      floorMovement->_worldRotation = worldRotationQuatnerion;
      floorMovement->_inverseWorldRotation = inverseWorldRotation;

      worldRotationQuatnerion = worldRotationQuatnerion * localRotation;
      transform->set_localRotation(worldRotationQuatnerion);
    } else {
      transform->set_localRotation(NEVector::Quaternion(transform->get_localRotation()) * localRotation);
    }
  }

  auto scale = NEVector::Vector3(ad.objectData.scaleX.value_or(1.0f), ad.objectData.scaleY.value_or(1.0f),
                                 ad.objectData.scaleZ.value_or(1.0f));
  ad.internalScale = scale;
  transform->set_localScale(scale);

  Vector3 moveStartPos = noteSpawnData->moveStartOffset;
  Vector3 moveEndPos = noteSpawnData->moveEndOffset;
  Vector3 jumpEndPos = noteSpawnData->jumpEndOffset;
  auto movement = VariableMovementW(self->_noteMovement->_variableMovementDataProvider);
  float jumpGravity = movement.CalculateCurrentNoteJumpGravity(noteSpawnData->gravityBase);
  float halfJumpDuration = movement.halfJumpDuration;

  float zOffset = self->_noteMovement->_zOffset;
  moveStartPos.z += zOffset;
  moveEndPos.z += zOffset;
  jumpEndPos.z += zOffset;

  ad.endRotation = endRotation;
  ad.moveStartPos = moveStartPos;
  ad.moveEndPos = moveEndPos;
  ad.jumpEndPos = jumpEndPos;
  ad.worldRotation = self->get_worldRotation();
  ad.localRotation = localRotation;

  float startVerticalVelocity = jumpGravity * halfJumpDuration;
  float yOffset =
      (startVerticalVelocity * halfJumpDuration) - (jumpGravity * halfJumpDuration * halfJumpDuration * 0.5f);
  ad.noteOffset = Vector3(jumpEndPos.x, moveEndPos.y + yOffset, 0);

  self->Update();
}

MAKE_HOOK_MATCH(NoteController_ManualUpdate, &NoteController::ManualUpdate, void, NoteController* self) {

  if (!Hooks::isNoodleHookEnabled()) return NoteController_ManualUpdate(self);

  noteUpdateAD = nullptr;
  noteTracks.clear();

  static auto CustomKlass = classof(CustomJSONData::CustomNoteData*);

  if (!il2cpp_functions::class_is_assignable_from(CustomKlass,
                                                   self->_noteData->klass)) {
    return NoteController_ManualUpdate(self);
  }

  auto* customNoteData = reinterpret_cast<CustomJSONData::CustomNoteData*>(self->_noteData);
  if (!customNoteData->customData) {
    noteUpdateAD = nullptr;
    noteTracks.clear();
    return NoteController_ManualUpdate(self);
  }

  // TODO: Cache deserialized animation data
  // if (!customData.HasMember("_animation")) {
  //     NoteController_Update(self);
  //     return;
  // }

  BeatmapObjectAssociatedData& ad = getAD(customNoteData->customData);
  auto const& tracks = TracksAD::getAD(customNoteData->customData).tracks;

  noteUpdateAD = &ad;
  noteTracks = tracks;
  if (noteTracks.empty() && !ad.animationData.parsed) {
    return NoteController_ManualUpdate(self);
  }

  NoteJump* noteJump = self->_noteMovement->_jump;
  NoteFloorMovement* floorMovement = self->_noteMovement->_floorMovement;
  VariableMovementW variableMovementDataProvider = self->_noteMovement->_variableMovementDataProvider;

  auto time = NoodleExtensions::getTimeProp(noteTracks);
  float normalTime;
  if (time) {
    normalTime = time.value();
  } else {
    float jumpDuration = variableMovementDataProvider.jumpDuration;
    float elapsedTime = TimeSourceHelper::getSongTime(noteJump->_audioTimeSyncController) -
                        (customNoteData->_time_k__BackingField - (jumpDuration * 0.5f));
    normalTime = elapsedTime / jumpDuration;
  }

  // auto context = TracksAD::getBeatmapAD(NECaches::customBeatmapData->customData).internal_tracks_context;
  AnimationHelper::ObjectOffset offset = AnimationHelper::GetObjectOffset(ad.animationData, noteTracks, normalTime);

  if (offset.positionOffset.has_value()) {
    auto const& offsetPos = *offset.positionOffset;
    floorMovement->_moveStartOffset = ad.moveStartPos + offsetPos;
    floorMovement->_moveEndOffset = ad.moveEndPos + offsetPos;
    noteJump->_startOffset = ad.moveEndPos + offsetPos;
    noteJump->_endOffset = ad.jumpEndPos + offsetPos;
    noteJump->_startPos = NEVector::Vector3(variableMovementDataProvider.moveEndPosition) + noteJump->_startOffset;
    noteJump->_endPos = NEVector::Vector3(variableMovementDataProvider.jumpEndPosition) + noteJump->_endOffset;
  }

  auto transform = self->get_transform();

  if (offset.scaleOffset.has_value()) {
    transform->set_localScale(*offset.scaleOffset * ad.internalScale);
  }

  if (offset.rotationOffset.has_value() || offset.localRotationOffset.has_value()) {
    NEVector::Quaternion worldRotation = ad.worldRotation;
    NEVector::Quaternion localRotation = ad.localRotation;

    NEVector::Quaternion worldRotationQuaternion = worldRotation;
    if (offset.rotationOffset.has_value()) {
      worldRotationQuaternion = worldRotationQuaternion * *offset.rotationOffset;
      NEVector::Quaternion inverseWorldRotation = NEVector::Quaternion::Inverse(worldRotationQuaternion);
      noteJump->_worldRotation = worldRotationQuaternion;
      noteJump->_inverseWorldRotation = inverseWorldRotation;
      floorMovement->_worldRotation = worldRotationQuaternion;
      floorMovement->_inverseWorldRotation = inverseWorldRotation;
    }

    worldRotationQuaternion = worldRotationQuaternion * localRotation;

    if (offset.localRotationOffset.has_value()) {
      worldRotationQuaternion = worldRotationQuaternion * *offset.localRotationOffset;
    }

    transform->set_localRotation(worldRotationQuaternion);
  }

  auto& noteCache = NECaches::getNoteCache(self);

  bool noteDissolveConfig = getNEConfig().enableNoteDissolve.GetValue();
  bool hasDissolveOffset = offset.dissolve.has_value() || offset.dissolveArrow.has_value();
  // Dissolve values are visibility values: zero is fully invisible and one is
  // fully visible. Keep the cutout-capable material selected for every partial
  // dissolve, including the fully-hidden boundary.
  bool isDissolving = (offset.dissolve.has_value() && *offset.dissolve < 1.0f) ||
                      (offset.dissolveArrow.has_value() && *offset.dissolveArrow < 1.0f);
  if (hasDissolveOffset && noteCache.dissolveEnabled != isDissolving && noteDissolveConfig) {
    // TODO: reimplement material switching
    ArrayW<ConditionalMaterialSwitcher*> materialSwitchers = noteCache.conditionalMaterialSwitchers;
    for (auto* materialSwitcher : materialSwitchers) {
      if (materialSwitcher == nullptr || materialSwitcher->_renderer == nullptr) continue;
      materialSwitcher->_renderer->set_sharedMaterial(isDissolving ? materialSwitcher->_material1
                                                                   : materialSwitcher->_material0);
    }
    noteCache.dissolveEnabled = isDissolving;
  }

  if (offset.dissolve.has_value()) {
    CutoutEffect* cutoutEffect = NECaches::GetCutout(self, noteCache);
    if (cutoutEffect == nullptr) {
      static bool loggedMissingCutout = false;
      if (!loggedMissingCutout) {
        loggedMissingCutout = true;
        NELogger::Logger.warn("Skipping note dissolve: the note has no CutoutEffect");
      }
    } else {
      if (noteDissolveConfig) {
        cutoutEffect->SetCutout(1 - *offset.dissolve);
      } else {
        cutoutEffect->SetCutout(*offset.dissolve >= 0 ? 0 : 1);
      }
    }
  }

  static auto* gameNoteControllerClass = classof(GameNoteController*);
  static auto* bombNoteControllerClass = classof(BombNoteController*);

  if (il2cpp_functions::class_is_assignable_from(gameNoteControllerClass, self->klass)) {
    if (offset.dissolveArrow.has_value() && self->_noteData->colorType != ColorType::None) {
      auto disappearingArrowController = NECaches::GetDisappearingArrowController((GameNoteController*)self, noteCache);
      if (disappearingArrowController != nullptr) {
        if (noteDissolveConfig) {
          disappearingArrowController->SetArrowTransparency(*offset.dissolveArrow);
        } else {
          disappearingArrowController->SetArrowTransparency(*offset.dissolveArrow >= 0 ? 1 : 0);
        }
      }
    }
  }

  if (il2cpp_functions::class_is_assignable_from(gameNoteControllerClass, self->klass) ||
      il2cpp_functions::class_is_assignable_from(bombNoteControllerClass, self->klass)) {
    if (offset.cuttable.has_value()) {
      bool enabled = *offset.cuttable >= 1;

      if (il2cpp_functions::class_is_assignable_from(gameNoteControllerClass, self->klass)) {
        // Match PC Noodle: authored interactable animation applies to normal
        // notes too. A statically uninteractable object remains non-cuttable.
        if (ad.objectData.uninteractable.value_or(false)) enabled = false;
        auto* gameNoteController = reinterpret_cast<GameNoteController*>(self);
        SetGameNoteCuttable(gameNoteController, enabled);
      } else if (il2cpp_functions::class_is_assignable_from(bombNoteControllerClass, self->klass)) {
        auto* bombNoteController = reinterpret_cast<BombNoteController*>(self);
        CuttableBySaber* cuttable = bombNoteController->_cuttableBySaber;
        if (cuttable && cuttable->get_canBeCut() != enabled) {
          cuttable->set_canBeCut(enabled);
        }
      }
    }
  }

  NoteController_ManualUpdate(self);

  // NoteJump.ManualUpdate will be the last place this is used after it was set in
  // NoteController.ManualUpdate. To make sure it doesn't interfere with future notes, it's set
  // back to null
  noteUpdateAD = nullptr;
  noteTracks.clear();
}

MAKE_HOOK_MATCH(NoteController_SendNoteWasCutEvent_LinkedNotes, &NoteController::SendNoteWasCutEvent, void,
                NoteController* self, ByRef<::GlobalNamespace::NoteCutInfo> noteCutInfo) {

  auto* customNoteData = il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(self->_noteData).value_or(nullptr);
  if (Hooks::isNoodleHookEnabled() && customNoteData && customNoteData->customData) {
    BeatmapObjectAssociatedData& ad = getAD(customNoteData->customData);

    if (ad.objectData.disableBadCutDirection || ad.objectData.disableBadCutSpeed || ad.objectData.disableBadCutSaberType) {
      noteCutInfo->directionOK = true;
      noteCutInfo->speedOK = true;
      noteCutInfo->saberTypeOK = true;
    }
  }

  NoteController_SendNoteWasCutEvent_LinkedNotes(self, noteCutInfo);

  if (!Hooks::isNoodleHookEnabled()) return;

  if (!customNoteData || !customNoteData->customData) {
    return;
  }

  BeatmapObjectAssociatedData& ad = getAD(customNoteData->customData);

  auto link = ad.objectData.link;
  if (!link) return;

  auto& list = linkedNotes[*link];

  list.erase(self);
  linkedLinkedNotes.erase(self);

  auto cuts = list | Sombrero::Linq::Functional::Select([&](auto const& noteController) {
                return std::pair(
                    noteController,
                    NoteCutInfo(noteController->_noteData, noteCutInfo->speedOK, noteCutInfo->directionOK,
                                noteCutInfo->saberTypeOK, noteCutInfo->wasCutTooSoon, noteCutInfo->saberSpeed,
                                noteCutInfo->saberDir, noteCutInfo->saberType, noteCutInfo->timeDeviation,
                                noteCutInfo->cutDirDeviation, noteCutInfo->cutPoint, noteCutInfo->cutNormal,
                                noteCutInfo->cutDistanceToCenter, noteCutInfo->cutAngle, noteCutInfo->worldRotation,
                                noteCutInfo->inverseWorldRotation, noteCutInfo->noteRotation, noteCutInfo->notePosition,
                                noteCutInfo->saberMovementData));
              }) |
              Sombrero::Linq::Functional::ToVector();

  for (auto const& note : list) {
    linkedLinkedNotes.erase(note);
  }
  list.clear();

  for (auto& [noteController, cutInfo] : cuts) {
    auto ref = ByRef<NoteCutInfo>(cutInfo);
    noteController->SendNoteWasCutEvent(ref);
  }
}
void ClearLinkedNote(GlobalNamespace::NoteController* noteController) {
  auto linkedLinkedIt = linkedLinkedNotes.find(noteController);
  if (linkedLinkedIt != linkedLinkedNotes.end()) {
    linkedLinkedIt->second->erase(noteController);
    linkedLinkedNotes.erase(linkedLinkedIt);
  }
}

MAKE_HOOK_MATCH(BurstSliderGameNoteController_ManualUpdate,
                &BurstSliderGameNoteController::ManualUpdate, void,
                BurstSliderGameNoteController* self) {
  if (self == nullptr) return;

  // Beat Saber 1.40.8's burst controller bypasses the NoteController hook when
  // it runs its own ManualUpdate. Invoke the base implementation explicitly so
  // Noodle position/dissolve animation is applied, then restore chain collider
  // sizing exactly as the game controller expects.
  static auto const* manualUpdate =
      il2cpp_utils::il2cpp_type_check::MetadataGetter<&NoteController::ManualUpdate>::methodInfo();
  il2cpp_utils::RunMethodRethrow<void, false>(self, manualUpdate);
  self->SetBigCuttableColliderSize();
}

void InstallNoteControllerHooks() {
  INSTALL_HOOK(NELogger::Logger, NoteController_Init);
  INSTALL_HOOK(NELogger::Logger, NoteController_ManualUpdate);

  INSTALL_HOOK(NELogger::Logger, NoteController_SendNoteWasCutEvent_LinkedNotes);
  INSTALL_HOOK(NELogger::Logger, BurstSliderGameNoteController_ManualUpdate);
}

NEInstallHooks(InstallNoteControllerHooks);
