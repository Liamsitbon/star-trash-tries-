#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(rel):
    return (ROOT / rel).read_text(errors='replace')

parent = read('src/Animation/ParentObject.cpp')
player = read('src/Animation/PlayerTrack.cpp')
player_h = read('include/Animation/PlayerTrack.h')
scene = read('src/Hooks/SceneTransition/SceneTransitionHelper.cpp')
sound = read('src/Hooks/FakeNotes/NoteCutSoundEffectManager.cpp')

remove_fn = parent.split('static void RemoveCallback', 1)[1].split('template <typename F>', 1)[0]
assert 'gameObjectModificationCallbacks[pair]' not in remove_fn, 'unsafe callback operator[] removal returned'
assert 'gameObjectModificationCallbacks.find(pair)' in remove_fn
assert 'RemoveGameObjectCallback(callback)' in parent, 'runtime parent callback cleanup missing'

assert 'static Action* didPauseEventAction' not in player
assert 'System::Action* didPauseEventAction' in player_h
assert 'System::Action* didResumeEventAction' in player_h
assert 'remove_didPauseEvent(didPauseEventAction)' in player
assert 'remove_didResumeEvent(didResumeEventAction)' in player
assert 'PlayerVRControllersManager/left controller not found' in player
assert 'if (playerTrack->trackController)' in player

assert 'bool blockConflict = conflict && getNEConfig().disableOnMappingExtensionsConflict.GetValue();' in scene
assert 'noodleRequirement && !blockConflict' in scene

assert 'static void SyncFrameBudget()' in sound
assert 'std::deque<NoteController*>' in sound
assert 'hitsoundQueue.front()' in sound and 'hitsoundQueue.pop_front()' in sound
assert 'cutCount += notesRemaining' not in sound, 'queued hitsounds are double-counted'
# Budget sync must happen in coroutine, before queue size is used.
coroutine = sound.split('custom_types::Helpers::Coroutine AddNotesLater()', 1)[1].split('MAKE_HOOK_MATCH', 1)[0]
assert 'SyncFrameBudget();' in coroutine

print('Additional 1.8.9 audit checks: PASS')
