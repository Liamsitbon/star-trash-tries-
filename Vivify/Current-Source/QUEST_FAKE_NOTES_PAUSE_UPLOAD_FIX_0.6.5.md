# Vivify Quest 0.6.5: fake notes, pause clock, 42 Flux and score submission

Date: 2026-08-13

This release pairs Vivify 0.6.5 with Noodle Extensions Quest 1.8.5. The fixes
are intentionally split at the ownership boundary: Noodle loads V3 fake
beatmap objects, while Vivify consumes the resulting beatmap objects and owns
the visual runtime, cameras and pause clock.

## Source and map evidence

- The official Quest Noodle Extensions repository still listed fake notes and
  corrupted BeatLeader replays as known issues. Its legacy late-transform path
  was also marked as needing a rewrite.
- The official CustomJSONData V3 model exposes fake arrays in save-data custom
  data. A fake object must enter the normal typed save-data list before the
  CustomJSONData conversion if downstream mods are expected to see it like a
  normal beatmap object.
- Murder Plot `45d7f` contains 912 real notes, 3,851 fake color notes and 51
  fake obstacles. All audited fake notes are authored as uninteractable.
- 42 Flux `43999` ExpertPlus contains 877 real color notes and 1,286 fake color
  notes. The fake notes are track-animated and uninteractable, so losing them
  removes a large part of the authored note art, including late-song designs.
- Aeroluna's official 42 Flux breakdown confirms that the opening includes a
  backwards clock, structure, glowing rods, notes and a film-grain transition;
  it is not intended to be a featureless black screen. It also documents the
  secondary note-camera/depth composition and the eclipse scene.

Primary references:

- <https://github.com/bsq-ports/NoodleExtensions>
- <https://github.com/Aeroluna/CustomJSONData>
- <https://aeroluna.dev/42-flux>
- <https://github.com/BeatLeader/beatleader-qmod>

The downloaded map audit and extracted timing summaries are preserved under
`diagnostics/2026-08-13-murder-plot-42-flux-source-audit/`.

## Noodle Extensions 1.8.5 integration fix

`HandleFakeObjects` now injects V3 `_fakeColorNotes`, `_fakeBombNotes`,
`_fakeObstacles`, `_fakeBurstSliders` and `_fakeSliders` into their normal
typed save-data lists during the CustomJSONData parser callback. Every injected
object receives the `NE_fake` marker before deserialization.

Injection is idempotent: a repeated parser callback sees the top-level marker
and returns without duplicating the typed lists. Missing or non-object per-item
`customData` is normalized to an object before the internal marker is added.

The old post-conversion path remains only as a compatibility fallback. A
top-level `NE_fakeObjectsInjected` marker prevents the same objects from being
added again after conversion. If marker propagation is lost, V3
`uninteractable: true` is also treated as fake/no-score. The fake-obstacle type
guard was corrected, and fake/uninteractable notes are explicitly uncuttable
and use `NoScore`.

This gives Vivify, Tracks and replay recorders one consistent beatmap object
graph. It fixes the failure mode in which Vivify saw only the 877 real 42 Flux
notes while 1,286 authored fake-note visuals arrived too late or behaved like
ordinary scoring notes.

## Pause, Quest Home and sleep clock fix

The previous pause handler called lifecycle `Suspend()` before the regular
runtime update could set synchronized Animator and ParticleSystem speeds to
zero. Because suspension also stops `Runtime::Update`, background objects could
continue on Unity time while the song was paused. Continue then exposed the
advanced background immediately.

0.6.5 now:

1. tracks pause-menu, `OnApplicationPause` and `OnApplicationFocus` as separate
   pause sources;
2. immediately freezes synchronized animators and particles and pauses video
   players before suspending runtime updates;
3. disables Vivify blit and secondary-camera work while paused;
4. restores the exact authored audio time scale and invalidates the song-time
   cache before normal updates resume;
5. keeps gameplay renderer ordering across pause, while full scene teardown
   still restores every modified sorting order.

This covers the in-game pause menu, the Quest system menu, sleep/wake and focus
loss. Nested pause sources are combined, so returning from the Quest system UI
does not resume effects while the Beat Saber pause menu is still open.

## 42 Flux opening and eclipse readability

The backwards-clock prefab starts slightly before beat zero. The old catch-up
filter classified it as historical and skipped it. Prefabs whose authored time
is within one second before the current song time are now prewarmed immediately;
older historical prefabs are still skipped to avoid replaying a whole map when
starting at a custom practice time.

The runtime still contained the intended gameplay `sortingOrder` bookkeeping,
but the refactored note/saber/replacement paths no longer called it. 0.6.5
restores those calls for stock note renderers involved in a Vivify assignment,
custom replacement renderers, debris, sabers and custom saber trails. This is a
narrow protection against late transparent foreground geometry such as the
eclipse moon covering playable cubes. The old extra gameplay overlay camera was
not restored because duplicating XR gameplay rendering would add stereo,
depth-buffer and performance risk on Quest.

## BeatLeader and ScoreSaber uploads

Vivify previously called `MetaCore::Game::SetScoreSubmission("Vivify", false)`
whenever a Vivify map was selected. BeatLeader therefore saved a local replay
but did not enter its posting path. 0.6.5 removes that self-imposed block and
sets Vivify's own submission flag to true on every level selection.

This does not bypass leaderboard integrity rules. Noodle fake notes remain
`NoScore`; BeatLeader's recorder excludes no-score misses, and BeatLeader,
ScoreSaber, MetaCore, other gameplay mods and the leaderboard servers retain
their own eligibility and validation decisions. The change only states that
Vivify's presentation effects do not themselves alter scoring.

## Validation boundary

Source inspection and Android ARM64 builds can prove that the code compiles and
that package metadata is internally consistent. They cannot prove rendering,
pause behavior or a successful server-side score upload. Those outcomes must be
tested with the exact Vivify 0.6.5 and Noodle Extensions 1.8.5 QMODs on a Quest,
using `STABILITY_TESTS_0.6.5.md`.
