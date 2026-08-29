#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "UnityEngine/Time.hpp"

#include "FakeNoteHelper.h"
#include "NEHooks.h"
#include "SharedUpdate.h"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "custom-types/shared/coroutine.hpp"

#include <deque>

using namespace GlobalNamespace;
using namespace UnityEngine;

static NoteCutSoundEffectManager* currentSoundEffectManager;
static int lastFrame = -1;
static int cutCount = 0;
static int const maxNotesPerFrame = 30;
static std::deque<NoteController*> hitsoundQueue;

static void SyncFrameBudget() {
  int frameCount = Time::get_frameCount();
  if (frameCount != lastFrame) {
    lastFrame = frameCount;
    cutCount = 0;
  }
}

static bool ProcessHitSound(NoteController* noteController) {
  SyncFrameBudget();
  if (cutCount >= maxNotesPerFrame) return false;
  cutCount++;
  return true;
}

custom_types::Helpers::Coroutine AddNotesLater() {
  while (currentSoundEffectManager) {
    SyncFrameBudget();

    if (!hitsoundQueue.empty()) {
      int notesRemaining = std::clamp(maxNotesPerFrame - cutCount, 0, (int)hitsoundQueue.size());

      for (int i = 0; i < notesRemaining; i++) {
        auto noteController = hitsoundQueue.front();
        hitsoundQueue.pop_front();
        if (!noteController) continue;

        currentSoundEffectManager->HandleNoteWasSpawned(noteController);
      }

    }

    co_yield nullptr;
  }
  co_return;
}

MAKE_HOOK_MATCH(NoteCutSoundEffectManager_Start, &NoteCutSoundEffectManager::Start, void,
                NoteCutSoundEffectManager* self) {
  if (!Hooks::isNoodleHookEnabled()) return NoteCutSoundEffectManager_Start(self);

  currentSoundEffectManager = self;
  lastFrame = -1;
  cutCount = 0;
  hitsoundQueue.clear();
  self->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(AddNotesLater()));
  NoteCutSoundEffectManager_Start(self);
}

MAKE_HOOK_MATCH(NoteCutSoundEffectManager_HandleNoteWasSpawned, &NoteCutSoundEffectManager::HandleNoteWasSpawned, void,
                NoteCutSoundEffectManager* self, NoteController* noteController) {
  if (!Hooks::isNoodleHookEnabled()) return NoteCutSoundEffectManager_HandleNoteWasSpawned(self, noteController);

  if (!FakeNoteHelper::GetFakeNote(noteController->_noteData)) {
    if (ProcessHitSound(noteController)) {
      NoteCutSoundEffectManager_HandleNoteWasSpawned(self, noteController);
    } else {
      hitsoundQueue.emplace_back(noteController);
    }
  }
}

void InstallNoteCutSoundEffectManagerHooks() {
  INSTALL_HOOK(NELogger::Logger, NoteCutSoundEffectManager_Start);
  INSTALL_HOOK(NELogger::Logger, NoteCutSoundEffectManager_HandleNoteWasSpawned);
}
NEInstallHooks(InstallNoteCutSoundEffectManagerHooks);