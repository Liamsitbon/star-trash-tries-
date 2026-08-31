#include "NexoraRuntime.hpp"
#include "main.hpp"

#include <exception>

#include "GlobalNamespace/GamePause.hpp"
#include "GlobalNamespace/GameScenesManager.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/ScenesTransitionSetupDataSO.hpp"
#include "System/Action.hpp"
#include "System/Collections/Generic/List_1.hpp"
#include "Zenject/DiContainer.hpp"

namespace {

template <typename Action>
void RunNexoraHookBoundary(char const* hookName, Action&& action) noexcept {
  try {
    action();
  } catch (std::exception const& exception) {
    try {
      PaperLogger.error("Nexora hook '{}' failed safely: {}", hookName,
                        exception.what());
    } catch (...) {
    }
  } catch (...) {
    try {
      PaperLogger.error("Nexora hook '{}' failed safely", hookName);
    } catch (...) {
    }
  }
}

MAKE_HOOK_MATCH(PauseController_Pause, &GlobalNamespace::PauseController::Pause, void,
                GlobalNamespace::PauseController* self) {
  RunNexoraHookBoundary("PauseController::Pause", [] {
    Nexora::Runtime::Instance().SetPaused(true);
  });
  PauseController_Pause(self);
}

MAKE_HOOK_MATCH(PauseMenuManager_RestartButtonPressed,
                &GlobalNamespace::PauseMenuManager::RestartButtonPressed, void,
                GlobalNamespace::PauseMenuManager* self) {
  RunNexoraHookBoundary("PauseMenuManager::RestartButtonPressed", [] {
    Nexora::Runtime::Instance().HandleGameplayRestart();
  });
  PauseMenuManager_RestartButtonPressed(self);
}

MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidFinishResumeAnimation,
                &GlobalNamespace::PauseController::HandlePauseMenuManagerDidFinishResumeAnimation,
                void, GlobalNamespace::PauseController* self) {
  PauseController_HandlePauseMenuManagerDidFinishResumeAnimation(self);
  RunNexoraHookBoundary("PauseController::DidFinishResumeAnimation", [] {
    Nexora::Runtime::Instance().SetPaused(false);
  });
}

MAKE_HOOK_MATCH(GamePause_Resume, &GlobalNamespace::GamePause::Resume, void,
                GlobalNamespace::GamePause* self) {
  GamePause_Resume(self);
  RunNexoraHookBoundary("GamePause::Resume", [] {
    Nexora::Runtime::Instance().SetPaused(false);
  });
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
  RunNexoraHookBoundary("GameScenesManager::ScenesTransitionCoroutine", [&] {
    if (scenesToDismiss != nullptr && scenesToDismiss->get_Count() > 0) {
      Nexora::Runtime::Instance().HandleScenesWillDismiss();
    }
  });
  return GameScenesManager_ScenesTransitionCoroutine(
      self, newScenesTransitionSetupData, scenesToPresent, presentType, scenesToDismiss,
      dismissType, minDuration, canTriggerGarbageCollector, afterMinDurationCallback,
      extraBindingsCallback, finishCallback);
}

}  // namespace

namespace Nexora {

void LateLoad() {
  // Install the scene-retirement and pause boundaries before advertising the
  // SongCore capability or subscribing to map events. If any required hook
  // fails, Scotland2's outer late_load boundary leaves Nexora unavailable
  // instead of accepting a map with an incomplete lifecycle.
  INSTALL_HOOK(PaperLogger, GameScenesManager_ScenesTransitionCoroutine);
  INSTALL_HOOK(PaperLogger, PauseController_Pause);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_RestartButtonPressed);
  INSTALL_HOOK(PaperLogger, PauseController_HandlePauseMenuManagerDidFinishResumeAnimation);
  INSTALL_HOOK(PaperLogger, GamePause_Resume);
  Runtime::Instance().LateLoad();
  PaperLogger.info("Nexora hooks installed");
}

}  // namespace Nexora
