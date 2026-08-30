#include "CinemaRuntime.hpp"

#include "main.hpp"

namespace {
bool gHooksInstalled = false;

MAKE_HOOK_MATCH(AudioTimeSyncController_StartSong,
                &GlobalNamespace::AudioTimeSyncController::StartSong, void,
                GlobalNamespace::AudioTimeSyncController* self,
                float startTimeOffset) {
  AudioTimeSyncController_StartSong(self, startTimeOffset);
  if (GetCinemaEnabled()) {
    CinemaQuest::Runtime::Instance().BeginGameplay(self, startTimeOffset);
  }
}

MAKE_HOOK_MATCH(AudioTimeSyncController_Update,
                &GlobalNamespace::AudioTimeSyncController::Update, void,
                GlobalNamespace::AudioTimeSyncController* self) {
  AudioTimeSyncController_Update(self);
  if (GetCinemaEnabled()) CinemaQuest::Runtime::Instance().Update();
}

MAKE_HOOK_MATCH(AudioTimeSyncController_StopSong,
                &GlobalNamespace::AudioTimeSyncController::StopSong, void,
                GlobalNamespace::AudioTimeSyncController* self) {
  AudioTimeSyncController_StopSong(self);
  CinemaQuest::Runtime::Instance().RetireGameplay(self, true);
}

MAKE_HOOK_MATCH(AudioTimeSyncController_OnDestroy,
                &GlobalNamespace::AudioTimeSyncController::OnDestroy, void,
                GlobalNamespace::AudioTimeSyncController* self) {
  AudioTimeSyncController_OnDestroy(self);
  // Scene teardown can invalidate Unity objects before peer hooks finish. At
  // this point only abandon raw references; scene ownership performs cleanup.
  CinemaQuest::Runtime::Instance().RetireGameplay(self, false);
}

MAKE_HOOK_MATCH(AudioTimeSyncController_Pause,
                &GlobalNamespace::AudioTimeSyncController::Pause, void,
                GlobalNamespace::AudioTimeSyncController* self) {
  AudioTimeSyncController_Pause(self);
  if (GetCinemaEnabled()) CinemaQuest::Runtime::Instance().SetPaused(true);
}

MAKE_HOOK_MATCH(AudioTimeSyncController_Resume,
                &GlobalNamespace::AudioTimeSyncController::Resume, void,
                GlobalNamespace::AudioTimeSyncController* self) {
  AudioTimeSyncController_Resume(self);
  if (GetCinemaEnabled()) CinemaQuest::Runtime::Instance().SetPaused(false);
}

}  // namespace

namespace CinemaQuest {

void InstallHooks() {
  if (gHooksInstalled) return;
  gHooksInstalled = true;
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_StartSong);
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Update);
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_StopSong);
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_OnDestroy);
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Pause);
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Resume);
  PaperLogger.info(
      "Cinema gameplay hooks installed: polling update, pause/resume, StopSong cleanup and pointer-only OnDestroy retirement");
}

}  // namespace CinemaQuest
