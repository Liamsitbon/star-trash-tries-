#include "VivifyRuntimeInternal.hpp"
#include "VivifyComponents.hpp"
#include "GlobalNamespace/UserInfo.hpp"
#include "GlobalNamespace/GameScenesManager.hpp"

using namespace Vivify;
using namespace std::string_view_literals;

namespace {

bool VivifyOwnsSaberTrailLifecycle() {
  auto& runtime = Runtime::Instance();
  return runtime.GetCurrentBeatmapData() != nullptr && !runtime.IsResetting();
}

bool ShouldSkipBrokenVivifyVRCenterAdjust(GlobalNamespace::VRCenterAdjust* self,
                                          std::string_view method) {
  auto& runtime = Runtime::Instance();
  // VRCenterAdjust is also a normal Beat Saber component. Only isolate broken
  // copies instantiated from a Vivify bundle during an active Vivify map.
  if (runtime.GetCurrentBeatmapData() == nullptr || runtime.IsResetting()) {
    return false;
  }
  if (!IsManagedAlive(self)) return true;

  bool const missingSettingsManager = self->____settingsManager == nullptr;
  bool const missingSettingsApplicator =
      !IsManagedAlive(self->____settingsApplicator.unsafePtr());
  if (!missingSettingsManager && !missingSettingsApplicator) return false;

  // Update is called every frame, so keep the diagnostic bounded while still
  // recording enough evidence to identify an invalid exported component.
  static int warningCount = 0;
  if (warningCount < 4 && (method != "Update"sv || warningCount == 0)) {
    ++warningCount;
    PaperLogger.warn(
        "Vivify skipped VRCenterAdjust.{} with missing _settingsManager={} _settingsApplicator={}",
        method, BoolText(missingSettingsManager), BoolText(missingSettingsApplicator));
  }
  return true;
}

MAKE_HOOK_MATCH(VRCenterAdjust_Start,
                &GlobalNamespace::VRCenterAdjust::Start,
                void,
                GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipBrokenVivifyVRCenterAdjust(self, "Start"sv)) return;
  VRCenterAdjust_Start(self);
}

MAKE_HOOK_MATCH(VRCenterAdjust_OnEnable,
                &GlobalNamespace::VRCenterAdjust::OnEnable,
                void,
                GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipBrokenVivifyVRCenterAdjust(self, "OnEnable"sv)) return;
  VRCenterAdjust_OnEnable(self);
}

MAKE_HOOK_MATCH(VRCenterAdjust_OnDisable,
                &GlobalNamespace::VRCenterAdjust::OnDisable,
                void,
                GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipBrokenVivifyVRCenterAdjust(self, "OnDisable"sv)) return;
  VRCenterAdjust_OnDisable(self);
}

MAKE_HOOK_MATCH(VRCenterAdjust_Update,
                &GlobalNamespace::VRCenterAdjust::Update,
                void,
                GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipBrokenVivifyVRCenterAdjust(self, "Update"sv)) return;
  VRCenterAdjust_Update(self);
}

MAKE_HOOK_MATCH(VRCenterAdjust_ResetRoom,
                &GlobalNamespace::VRCenterAdjust::ResetRoom,
                void,
                GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipBrokenVivifyVRCenterAdjust(self, "ResetRoom"sv)) return;
  VRCenterAdjust_ResetRoom(self);
}

MAKE_HOOK_MATCH(VRCenterAdjust_SetRoomTransformOffset,
                &GlobalNamespace::VRCenterAdjust::SetRoomTransformOffset,
                void,
                GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipBrokenVivifyVRCenterAdjust(self, "SetRoomTransformOffset"sv)) return;
  VRCenterAdjust_SetRoomTransformOffset(self);
}

MAKE_HOOK_MATCH(SaberModelController_Init, &GlobalNamespace::SaberModelController::Init, void, GlobalNamespace::SaberModelController* self, UnityEngine::Transform* parent, GlobalNamespace::Saber* saber, UnityEngine::Color trailTintColor) {
  SaberModelController_Init(self, parent, saber, trailTintColor);
  Runtime::Instance().TrackSaberModel(self, saber, parent);
}

MAKE_HOOK_MATCH(GameNoteController_Init, &GlobalNamespace::GameNoteController::Init, void, GlobalNamespace::GameNoteController* self, GlobalNamespace::NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData, GlobalNamespace::NoteVisualModifierType noteVisualModifierType, float cutAngleTolerance, float uniformScale) {
  GameNoteController_Init(self, noteData, noteSpawnData, noteVisualModifierType, cutAngleTolerance, uniformScale);
  Runtime::Instance().ApplyNotePrefabFor(self);
}

MAKE_HOOK_MATCH(BombNoteController_Init, &GlobalNamespace::BombNoteController::Init, void, GlobalNamespace::BombNoteController* self, GlobalNamespace::NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData) {
  BombNoteController_Init(self, noteData, noteSpawnData);
  Runtime::Instance().ApplyNotePrefabFor(self);
}

MAKE_HOOK_MATCH(BurstSliderGameNoteController_Init, &GlobalNamespace::BurstSliderGameNoteController::Init, void, GlobalNamespace::BurstSliderGameNoteController* self, GlobalNamespace::NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData, GlobalNamespace::NoteVisualModifierType noteVisualModifierType, float uniformScale) {
  BurstSliderGameNoteController_Init(self, noteData, noteSpawnData, noteVisualModifierType, uniformScale);
  Runtime::Instance().ApplyNotePrefabFor(self);
}

MAKE_HOOK_MATCH(NoteCutCoreEffectsSpawner_SpawnNoteCutEffect, &GlobalNamespace::NoteCutCoreEffectsSpawner::SpawnNoteCutEffect, void, GlobalNamespace::NoteCutCoreEffectsSpawner* self, ByRef<GlobalNamespace::NoteCutInfo> noteCutInfo, GlobalNamespace::NoteController* noteController, int32_t sparkleParticlesCount, int32_t explosionParticlesCount) {

  bool const customDebris = Runtime::Instance().GetCurrentBeatmapData() != nullptr &&
                            !Runtime::Instance().IsResetting() && !Runtime::Instance().IsReduceDebrisEnabled();
  if (customDebris) {
    Runtime::Instance().PushActiveDebrisPrefabs(Runtime::Instance().FindAssignedDebrisPrefabs(noteCutInfo->noteData));
  }
  NoteCutCoreEffectsSpawner_SpawnNoteCutEffect(self, noteCutInfo, noteController, sparkleParticlesCount, explosionParticlesCount);
  if (customDebris) {
    Runtime::Instance().PopActiveDebrisPrefabs();
  }
}

MAKE_HOOK_MATCH(NoteDebris_Init, &GlobalNamespace::NoteDebris::Init, void, GlobalNamespace::NoteDebris* self, GlobalNamespace::ColorType colorType, UnityEngine::Vector3 notePos, UnityEngine::Quaternion noteRot, UnityEngine::Vector3 noteMoveVec, UnityEngine::Vector3 noteScale, UnityEngine::Vector3 positionOffset, UnityEngine::Quaternion rotationOffset, UnityEngine::Vector3 cutPoint, UnityEngine::Vector3 cutNormal, UnityEngine::Vector3 force, UnityEngine::Vector3 torque, float lifeTime, UnityEngine::Vector3 cutoutOffset, bool forceOnlySimplePhysics) {
  NoteDebris_Init(self, colorType, notePos, noteRot, noteMoveVec, noteScale, positionOffset, rotationOffset, cutPoint, cutNormal, force, torque, lifeTime, cutoutOffset, forceOnlySimplePhysics);
  Runtime::Instance().RestoreDebrisVisuals(self);
  if (Runtime::Instance().GetCurrentBeatmapData() == nullptr || Runtime::Instance().IsResetting()) return;
  Runtime::Instance().ReplaceDebrisVisuals(self);
}

MAKE_HOOK_MATCH(AlwaysVisibleQuad_OnEnable, &GlobalNamespace::AlwaysVisibleQuad::OnEnable, void, GlobalNamespace::AlwaysVisibleQuad* self) {
  AlwaysVisibleQuad_OnEnable(self);
  Runtime::Instance().HandleAlwaysVisibleQuad(self);
}

// Port of desktop Vivify's SaberTrailRendererEnabler patch.  Beat Saber's
// stock enable/disable callbacks can deactivate the renderer independently of
// the followed custom trail.  Scope the override to an active Vivify beatmap so
// ordinary maps and other Quest mods retain the base-game lifecycle.
MAKE_HOOK_MATCH(SaberTrailRenderer_OnEnable,
                &GlobalNamespace::SaberTrailRenderer::OnEnable,
                void,
                GlobalNamespace::SaberTrailRenderer* self) {
  if (!VivifyOwnsSaberTrailLifecycle()) {
    SaberTrailRenderer_OnEnable(self);
  }
}

MAKE_HOOK_MATCH(SaberTrailRenderer_OnDisable,
                &GlobalNamespace::SaberTrailRenderer::OnDisable,
                void,
                GlobalNamespace::SaberTrailRenderer* self) {
  if (!VivifyOwnsSaberTrailLifecycle()) {
    SaberTrailRenderer_OnDisable(self);
  }
}

MAKE_HOOK_MATCH(SaberTrail_OnDisable,
                &GlobalNamespace::SaberTrail::OnDisable,
                void,
                GlobalNamespace::SaberTrail* self) {
  if (!VivifyOwnsSaberTrailLifecycle()) {
    SaberTrail_OnDisable(self);
    return;
  }
  auto* renderer = self->____trailRenderer.unsafePtr();
  if (IsManagedAlive(renderer)) {
    auto rendererObject = renderer->get_gameObject();
    if (IsManagedAlive(rendererObject.unsafePtr())) {
      rendererObject->SetActive(false);
    }
  }
}

MAKE_HOOK_MATCH(SaberTrail_OnEnable,
                &GlobalNamespace::SaberTrail::OnEnable,
                void,
                GlobalNamespace::SaberTrail* self) {
  if (!VivifyOwnsSaberTrailLifecycle()) {
    SaberTrail_OnEnable(self);
    return;
  }
  auto* renderer = self->____trailRenderer.unsafePtr();
  if (self->____inited && IsManagedAlive(renderer) &&
      self->____trailElementCollection != nullptr) {
    self->ResetTrailData();
    renderer->UpdateMesh(self->____trailElementCollection, self->____color);
  }
  if (IsManagedAlive(renderer)) {
    auto rendererObject = renderer->get_gameObject();
    if (IsManagedAlive(rendererObject.unsafePtr())) {
      rendererObject->SetActive(true);
    }
  }
}

MAKE_HOOK_MATCH(PauseController_Pause, &GlobalNamespace::PauseController::Pause, void, GlobalNamespace::PauseController* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseController_Pause(self);
}

MAKE_HOOK_MATCH(PauseMenuManager_ShowMenu, &GlobalNamespace::PauseMenuManager::ShowMenu, void, GlobalNamespace::PauseMenuManager* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseMenuManager_ShowMenu(self);
}

MAKE_HOOK_MATCH(PauseMenuManager_MenuButtonPressed, &GlobalNamespace::PauseMenuManager::MenuButtonPressed, void, GlobalNamespace::PauseMenuManager* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseMenuManager_MenuButtonPressed(self);
}

MAKE_HOOK_MATCH(PauseMenuManager_RestartButtonPressed, &GlobalNamespace::PauseMenuManager::RestartButtonPressed, void, GlobalNamespace::PauseMenuManager* self) {
  Runtime::Instance().HandleGameplayRestart();
  PauseMenuManager_RestartButtonPressed(self);
}

MAKE_HOOK_MATCH(ImageEffectController_OnRenderImage,
                &GlobalNamespace::ImageEffectController::OnRenderImage,
                void,
                GlobalNamespace::ImageEffectController* self,
                UnityEngine::RenderTexture* src,
                UnityEngine::RenderTexture* dest) {
  if (Runtime::Instance().ShouldSuppressOriginalImageEffect(self)) {
    // CameraApplier invokes this controller's callback at the desktop Vivify
    // boundary. Preserve Unity's image-effect chain here without rendering the
    // same main effect twice.
    if (!Runtime::Instance().CopyStereoRenderTexture(src, dest)) {
      UnityEngine::Graphics::Blit(src, dest);
    }
    return;
  }
  ImageEffectController_OnRenderImage(self, src, dest);
}

MAKE_HOOK_MATCH(
    GameScenesManager_ScenesTransitionCoroutine,
    &GlobalNamespace::GameScenesManager::ScenesTransitionCoroutine,
    System::Collections::IEnumerator*,
    GlobalNamespace::GameScenesManager* self,
    GlobalNamespace::ScenesTransitionSetupDataSO* newScenesTransitionSetupData,
    System::Collections::Generic::List_1<StringW>* scenesToPresent,
    GlobalNamespace::GameScenesManager_ScenePresentType presentType,
    System::Collections::Generic::List_1<StringW>* scenesToDismiss,
    GlobalNamespace::GameScenesManager_SceneDismissType dismissType,
    float_t minDuration,
    bool canTriggerGarbageCollector,
    System::Action* afterMinDurationCallback,
    System::Action_1<Zenject::DiContainer*>* extraBindingsCallback,
    System::Action_1<Zenject::DiContainer*>* finishCallback) {
  if (scenesToDismiss != nullptr && scenesToDismiss->get_Count() > 0) {
    Runtime::Instance().HandleScenesWillDismiss();
  }
  return GameScenesManager_ScenesTransitionCoroutine(
      self, newScenesTransitionSetupData, scenesToPresent, presentType,
      scenesToDismiss, dismissType, minDuration, canTriggerGarbageCollector,
      afterMinDurationCallback, extraBindingsCallback, finishCallback);
}

MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidPressMenuButton, &GlobalNamespace::PauseController::HandlePauseMenuManagerDidPressMenuButton, void, GlobalNamespace::PauseController* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseController_HandlePauseMenuManagerDidPressMenuButton(self);
}

MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidFinishResumeAnimation, &GlobalNamespace::PauseController::HandlePauseMenuManagerDidFinishResumeAnimation, void, GlobalNamespace::PauseController* self) {
  PauseController_HandlePauseMenuManagerDidFinishResumeAnimation(self);
  Runtime::Instance().SetPauseMenuActive(false);
}

MAKE_HOOK_MATCH(GamePause_Resume, &GlobalNamespace::GamePause::Resume, void, GlobalNamespace::GamePause* self) {
  GamePause_Resume(self);
  Runtime::Instance().SetPauseMenuActive(false);
}

}

namespace Vivify {

void LateLoad() {
  Runtime::Instance().LateLoad();
  INSTALL_HOOK(PaperLogger, SaberModelController_Init);
  INSTALL_HOOK(PaperLogger, GameNoteController_Init);
  INSTALL_HOOK(PaperLogger, BombNoteController_Init);
  INSTALL_HOOK(PaperLogger, BurstSliderGameNoteController_Init);
  INSTALL_HOOK(PaperLogger, NoteCutCoreEffectsSpawner_SpawnNoteCutEffect);
  INSTALL_HOOK(PaperLogger, NoteDebris_Init);
  INSTALL_HOOK(PaperLogger, AlwaysVisibleQuad_OnEnable);
  INSTALL_HOOK(PaperLogger, SaberTrailRenderer_OnEnable);
  INSTALL_HOOK(PaperLogger, SaberTrailRenderer_OnDisable);
  INSTALL_HOOK(PaperLogger, SaberTrail_OnDisable);
  INSTALL_HOOK(PaperLogger, SaberTrail_OnEnable);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_Start);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_OnEnable);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_OnDisable);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_Update);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_ResetRoom);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_SetRoomTransformOffset);
  INSTALL_HOOK(PaperLogger, PauseController_Pause);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_ShowMenu);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_MenuButtonPressed);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_RestartButtonPressed);
  INSTALL_HOOK(PaperLogger, ImageEffectController_OnRenderImage);
  INSTALL_HOOK(PaperLogger, GameScenesManager_ScenesTransitionCoroutine);
  INSTALL_HOOK(PaperLogger, PauseController_HandlePauseMenuManagerDidPressMenuButton);
  INSTALL_HOOK(PaperLogger, PauseController_HandlePauseMenuManagerDidFinishResumeAnimation);
  INSTALL_HOOK(PaperLogger, GamePause_Resume);
}

void RefreshMultipassRendering() {
  Runtime::Instance().RefreshMultipassRendering();
}

void RefreshIsolationSettings() {
  Runtime::Instance().RefreshIsolationSettings();
}

}
