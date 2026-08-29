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
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Pause);
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Resume);
  PaperLogger.info(
      "Cinema gameplay, pause and practice-sync hooks installed; global scene-transition hook intentionally disabled");
}

}  // namespace CinemaQuest
