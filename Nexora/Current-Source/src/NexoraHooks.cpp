#include "NexoraRuntime.hpp"
#include "main.hpp"

#include "GlobalNamespace/GamePause.hpp"
#include "GlobalNamespace/GameScenesManager.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/ScenesTransitionSetupDataSO.hpp"
#include "System/Action.hpp"
#include "System/Collections/Generic/List_1.hpp"
#include "Zenject/DiContainer.hpp"

namespace {

MAKE_HOOK_MATCH(PauseController_Pause, &GlobalNamespace::PauseController::Pause, void,
                GlobalNamespace::PauseController* self) {
  Nexora::Runtime::Instance().SetPaused(true);
  PauseController_Pause(self);
}

MAKE_HOOK_MATCH(PauseMenuManager_RestartButtonPressed,
                &GlobalNamespace::PauseMenuManager::RestartButtonPressed, void,
                GlobalNamespace::PauseMenuManager* self) {
  Nexora::Runtime::Instance().HandleGameplayRestart();
  PauseMenuManager_RestartButtonPressed(self);
}

MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidFinishResumeAnimation,
                &GlobalNamespace::PauseController::HandlePauseMenuManagerDidFinishResumeAnimation,
                void, GlobalNamespace::PauseController* self) {
  PauseController_HandlePauseMenuManagerDidFinishResumeAnimation(self);
  Nexora::Runtime::Instance().SetPaused(false);
}

MAKE_HOOK_MATCH(GamePause_Resume, &GlobalNamespace::GamePause::Resume, void,
                GlobalNamespace::GamePause* self) {
  GamePause_Resume(self);
  Nexora::Runtime::Instance().SetPaused(false);
}

MAKE_HOOK_MATCH(
    GameScenesManager_ScenesTransitionCoroutine,
    &GlobalNamespace::GameScenesManager::ScenesTransitionCoroutine,
    System::Collections::IEnumerator*, GlobalNamespace::GameScenesManager* self,
    GlobalNamespace::ScenesTransitionSetupDataSO* newScenesTransitionSetupData,
    System::Collections::Generic::List_1<StringW>* scenesToPresent,
    GlobalNamespace::GameScenesManager_ScenePresentType presentType,
    System::Collections::Generic::List_1<StringW>* scenesToDismiss,
    GlobalNamespace::GameScenesManager_SceneDismissType dismissType, float_t minDuration,
    bool canTriggerGarbageCollector, System::Action* afterMinDurationCallback,
    System::Action_1<Zenject::DiContainer*>* extraBindingsCallback,
    System::Action_1<Zenject::DiContainer*>* finishCallback) {
  if (scenesToDismiss != nullptr && scenesToDismiss->get_Count() > 0) {
    Nexora::Runtime::Instance().HandleScenesWillDismiss();
  }
  return GameScenesManager_ScenesTransitionCoroutine(
      self, newScenesTransitionSetupData, scenesToPresent, presentType, scenesToDismiss,
      dismissType, minDuration, canTriggerGarbageCollector, afterMinDurationCallback,
      extraBindingsCallback, finishCallback);
}

}  // namespace

namespace Nexora {

void LateLoad() {
  Runtime::Instance().LateLoad();
  INSTALL_HOOK(PaperLogger, PauseController_Pause);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_RestartButtonPressed);
  INSTALL_HOOK(PaperLogger, PauseController_HandlePauseMenuManagerDidFinishResumeAnimation);
  INSTALL_HOOK(PaperLogger, GamePause_Resume);
  INSTALL_HOOK(PaperLogger, GameScenesManager_ScenesTransitionCoroutine);
  PaperLogger.info("Nexora hooks installed");
}

}  // namespace Nexora

