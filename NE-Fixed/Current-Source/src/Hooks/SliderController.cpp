#include "NELogger.h"
#include "VariableMovementHelper.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/CutoutAnimateEffect.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/SliderController.hpp"
#include "GlobalNamespace/SliderIntensityEffect.hpp"
#include "GlobalNamespace/SliderMovement.hpp"
#include "System/Action.hpp"
#include "System/Action_1.hpp"

#include "Animation/AnimationHelper.h"
#include "AssociatedData.h"
#include "NECaches.h"
#include "NEHooks.h"
#include "NEUtils.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "tracks/shared/TimeSourceHelper.h"

using namespace GlobalNamespace;
using namespace NoodleExtensions;
using namespace UnityEngine;

void NECaches::ClearSliderCaches() { sliderCache.clear(); }

MAKE_HOOK_MATCH(SliderController_IsNoteStartOfThisSlider,
                &SliderController::IsNoteStartOfThisSlider, bool,
                SliderController* self, NoteData* noteData) {
  if (!Hooks::isNoodleHookEnabled() || self == nullptr || noteData == nullptr ||
      self->_sliderData == nullptr) {
    return SliderController_IsNoteStartOfThisSlider(self, noteData);
  }

  if (!Approximately(noteData->time, self->_sliderData->time) ||
      noteData->colorType != self->_sliderData->colorType) {
    return false;
  }

  static auto const* customSliderClass = classof(CustomJSONData::CustomSliderData*);
  static auto const* customNoteClass = classof(CustomJSONData::CustomNoteData*);
  if (self->_sliderData->klass != customSliderClass || noteData->klass != customNoteClass) {
    return SliderController_IsNoteStartOfThisSlider(self, noteData);
  }

  auto* customSliderData =
      reinterpret_cast<CustomJSONData::CustomSliderData*>(self->_sliderData);
  auto* customNoteData = reinterpret_cast<CustomJSONData::CustomNoteData*>(noteData);
  if (customSliderData->customData == nullptr || customNoteData->customData == nullptr) {
    return SliderController_IsNoteStartOfThisSlider(self, noteData);
  }

  BeatmapObjectAssociatedData& sliderData = getAD(customSliderData->customData);
  BeatmapObjectAssociatedData& noteAssociatedData = getAD(customNoteData->customData);
  if (!sliderData.parsed || !noteAssociatedData.parsed ||
      !NECaches::beatmapObjectSpawnController) {
    return SliderController_IsNoteStartOfThisSlider(self, noteData);
  }

  auto* movementData =
      NECaches::beatmapObjectSpawnController->beatmapObjectSpawnMovementData;
  if (movementData == nullptr) {
    return SliderController_IsNoteStartOfThisSlider(self, noteData);
  }

  int const offset = movementData->noteLinesCount / 2;
  float const headIndex = sliderData.objectData.startX.has_value()
                              ? *sliderData.objectData.startX + offset
                              : self->_sliderData->headLineIndex;
  float const noteIndex = noteAssociatedData.objectData.startX.has_value()
                              ? *noteAssociatedData.objectData.startX + offset
                              : noteData->lineIndex;
  float const headLayer = sliderData.objectData.startY.value_or(
      static_cast<int>(self->_sliderData->headLineLayer));
  float const noteLayer = noteAssociatedData.objectData.startY.value_or(
      static_cast<int>(noteData->noteLineLayer));

  return Approximately(headIndex, noteIndex) && Approximately(headLayer, noteLayer);
}

MAKE_HOOK_MATCH(SliderController_Init, &SliderController::Init, void,
                SliderController* self, SliderController::LengthType lengthType,
                SliderData* sliderData, ByRef<SliderSpawnData> sliderSpawnData,
                float noteUniformScale, float randomValue) {
  SliderController_Init(self, lengthType, sliderData, sliderSpawnData,
                        noteUniformScale, randomValue);

  if (!Hooks::isNoodleHookEnabled() || self == nullptr || sliderData == nullptr ||
      self->_sliderMovement == nullptr) {
    return;
  }

  static auto const* customSliderClass = classof(CustomJSONData::CustomSliderData*);
  if (sliderData->klass != customSliderClass) return;

  auto* customSliderData = reinterpret_cast<CustomJSONData::CustomSliderData*>(sliderData);
  if (customSliderData->customData == nullptr) return;

  BeatmapObjectAssociatedData& associatedData = getAD(customSliderData->customData);
  if (!associatedData.parsed) return;

  auto* transform = self->get_transform().unsafePtr();
  if (transform == nullptr) return;
  transform->set_localScale(NEVector::Vector3::one());

  auto& sliderCache = NECaches::getSliderCache(self->_sliderMovement);
  sliderCache.cutoutAnimateEffect = self->_cutoutAnimateEffect;
  if (sliderCache.cutoutAnimateEffect != nullptr) {
    // Controllers are pooled; never inherit a fully dissolved previous arc.
    sliderCache.cutoutAnimateEffect->SetCutout(0.0f);
  }

  NEVector::Quaternion localRotation = NEVector::Quaternion::identity();
  if (associatedData.objectData.localRotation) {
    localRotation = *associatedData.objectData.localRotation;
  }
  if (associatedData.objectData.rotation) {
    NEVector::Quaternion worldRotation = *associatedData.objectData.rotation;
    self->_sliderMovement->_worldRotation = worldRotation;
    transform->set_localRotation(worldRotation * localRotation);
  } else if (associatedData.objectData.localRotation) {
    transform->set_localRotation(NEVector::Quaternion(transform->get_localRotation()) *
                                 localRotation);
  }

  auto const scale = NEVector::Vector3(
      associatedData.objectData.scaleX.value_or(1.0f),
      associatedData.objectData.scaleY.value_or(1.0f),
      associatedData.objectData.scaleZ.value_or(1.0f));
  transform->set_localScale(scale);
  associatedData.internalScale = scale;
  associatedData.worldRotation = self->_sliderMovement->_worldRotation;
  associatedData.localRotation = localRotation;
}

MAKE_HOOK_MATCH(SliderMovement_ManualUpdate, &SliderMovement::ManualUpdate, void,
                SliderMovement* self) {
  if (!Hooks::isNoodleHookEnabled() || self == nullptr ||
      self->_sliderData == nullptr || self->_variableMovementDataProvider == nullptr) {
    return SliderMovement_ManualUpdate(self);
  }

  static auto const* customSliderClass = classof(CustomJSONData::CustomSliderData*);
  if (self->_sliderData->klass != customSliderClass) {
    return SliderMovement_ManualUpdate(self);
  }

  auto* customSliderData =
      reinterpret_cast<CustomJSONData::CustomSliderData*>(self->_sliderData);
  if (customSliderData->customData == nullptr) return SliderMovement_ManualUpdate(self);

  BeatmapObjectAssociatedData& associatedData = getAD(customSliderData->customData);
  auto const& tracks = TracksAD::getAD(customSliderData->customData).tracks;
  if (tracks.empty() && !associatedData.animationData.parsed) {
    return SliderMovement_ManualUpdate(self);
  }

  auto* transform = self->get_transform().unsafePtr();
  if (transform == nullptr) return SliderMovement_ManualUpdate(self);

  VariableMovementW const movement(self->_variableMovementDataProvider);
  float const headNoteTime = self->_sliderData->time;
  float const tailNoteTime = self->_sliderData->tailTime;
  float const jumpDuration = movement.jumpDuration;
  if (jumpDuration <= 0.0001f) return SliderMovement_ManualUpdate(self);

  float const duration = (jumpDuration * 0.75f) + (tailNoteTime - headNoteTime);
  float const halfJumpDuration = jumpDuration * 0.5f;
  float normalizedTime = 0.0f;
  float timeSinceTailNoteJump = 0.0f;

  if (auto const time = getTimeProp(tracks); time.has_value()) {
    normalizedTime = *time;
    self->_timeSinceHeadNoteJump = normalizedTime * duration;
    timeSinceTailNoteJump =
        (self->_timeSinceHeadNoteJump + (headNoteTime - halfJumpDuration)) -
        (tailNoteTime - halfJumpDuration);
  } else {
    float const songTime = TimeSourceHelper::getSongTime(self->_audioTimeSyncController);
    self->_timeSinceHeadNoteJump = songTime - (headNoteTime - halfJumpDuration);
    normalizedTime = duration > 0.0001f ? self->_timeSinceHeadNoteJump / duration : 0.0f;
    timeSinceTailNoteJump = songTime - (tailNoteTime - halfJumpDuration);
  }

  float const normalizedHeadTime = self->_timeSinceHeadNoteJump / jumpDuration;
  float const normalizedTailTime = timeSinceTailNoteJump / jumpDuration;
  normalizedTime = std::max(normalizedTime, 0.0f);

  auto const offset =
      AnimationHelper::GetObjectOffset(associatedData.animationData, tracks, normalizedTime);
  self->_localPosition = NEVector::Vector3::zero();
  NEVector::Quaternion worldRotation = self->_worldRotation;

  if (offset.rotationOffset || offset.localRotationOffset) {
    worldRotation = associatedData.worldRotation;
    NEVector::Quaternion localRotation = associatedData.localRotation;
    NEVector::Quaternion composedRotation = worldRotation;
    if (offset.rotationOffset) {
      composedRotation = composedRotation * *offset.rotationOffset;
      worldRotation = composedRotation;
    }
    composedRotation = composedRotation * localRotation;
    if (offset.localRotationOffset) {
      composedRotation = composedRotation * *offset.localRotationOffset;
    }
    transform->set_localRotation(composedRotation);
  }

  if (offset.scaleOffset) {
    transform->set_localScale(*offset.scaleOffset * associatedData.internalScale);
  }

  if (offset.dissolve) {
    auto* cutout = NECaches::getSliderCache(self).cutoutAnimateEffect;
    if (cutout != nullptr) cutout->SetCutout(1.0f - *offset.dissolve);
  }

  auto const definitePosition = AnimationHelper::GetDefinitePositionOffset(
      associatedData.animationData, tracks, normalizedTime);
  if (definitePosition) {
    transform->set_localPosition(*definitePosition);
  } else {
    if (offset.positionOffset) self->_localPosition = *offset.positionOffset;
    float const headOffsetZ = self->_sliderSpawnData.headNoteOffset.z;
    float const startZ = movement.moveEndPosition.z + headOffsetZ;
    float const endZ = movement.jumpEndPosition.z + headOffsetZ;
    self->_localPosition.z += std::lerp(startZ, endZ, normalizedHeadTime);
    transform->set_localPosition(worldRotation * self->_localPosition);
  }

  if (!self->_headDidMovePastCutMarkReported && normalizedHeadTime > 0.5f) {
    self->_headDidMovePastCutMarkReported = true;
    if (self->headDidMovePastCutMarkEvent) self->headDidMovePastCutMarkEvent->Invoke();
  }
  if (!self->_tailDidMovePastCutMarkReported && normalizedTailTime > 0.5f) {
    self->_tailDidMovePastCutMarkReported = true;
    if (self->tailDidMovePastCutMarkEvent) self->tailDidMovePastCutMarkEvent->Invoke();
  }
  if (!self->_movementEndReported && normalizedTailTime > 0.75f) {
    self->_movementEndReported = true;
    if (self->movementDidFinishEvent) self->movementDidFinishEvent->Invoke();
  }
  if (self->movementDidMoveEvent) {
    self->movementDidMoveEvent->Invoke(self->_timeSinceHeadNoteJump);
  }
}

void InstallSliderAnimationHooks() {
  INSTALL_HOOK(NELogger::Logger, SliderController_IsNoteStartOfThisSlider);
  INSTALL_HOOK(NELogger::Logger, SliderController_Init);
  INSTALL_HOOK(NELogger::Logger, SliderMovement_ManualUpdate);
}

NEInstallHooks(InstallSliderAnimationHooks);
