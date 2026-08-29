# Quest 3 validation matrix — Vivify 0.6.5 + Noodle Extensions 1.8.5

Use Beat Saber `1.40.8_7379`. Install the exact QMODs produced by this source
tree and preserve logs after each run. A host build is not a headset pass.

## Required log set

- `/sdcard/ModData/com.beatgames.beatsaber/Logs/Vivify.log`
- `/sdcard/ModData/com.beatgames.beatsaber/Logs/NoodleExtensions.log`
- the active BeatLeader and/or ScoreSaber log
- the main Paper/Beat Saber log
- Android tombstone or crash log if one is created

The expected Noodle log contains an early fake-object injection count and must
not show a second late insertion of the same V3 fake arrays.

## Test 1 — Murder Plot fake-note semantics

1. Start Murder Plot from beat zero on a supported difficulty.
2. Play through multiple dense fake-note sections.
3. Confirm that authored fake shapes are visible and animated.
4. Swing through and miss fake notes deliberately.
5. Confirm they do not behave as normal cuttable/scoring notes, do not create
   ordinary misses and do not corrupt the score/replay.
6. Confirm fake obstacles retain their intended behavior.

Failure evidence: timestamp, difficulty, video and the complete Noodle/Vivify
logs. Do not report only the visible symptom.

## Test 2 — 42 Flux opening

1. Start ExpertPlus from the normal Play button, not Custom Time.
2. At beat zero, confirm the backwards clock and authored opening geometry are
   present; the scene must not remain featureless black.
3. Confirm film grain/white transition and the next environment appear in time
   with the music and cubes.
4. Repeat once from a normal restart and once after returning to song select.

The log should show the slightly negative-time opening prefab prewarmed rather
than counted as an old historical prefab.

## Test 3 — 42 Flux fake designs and ending

1. Play the complete ExpertPlus map from the beginning.
2. Confirm fake-note based designs appear throughout the map.
3. Confirm special note designs do not silently fall back to stock notes.
4. Confirm the final authored cube sequence remains visible.
5. Deliberately avoid several fake visuals and confirm no normal scoring miss is
   generated for them.

## Test 4 — eclipse moon/sun visibility

At the eclipse scene (approximately 40 seconds into the 42 Flux segment), keep
the red and blue lanes in view. Confirm that:

- the moon still obscures the sun as authored;
- playable cubes and custom cube designs remain readable in front of the moon;
- neither eye loses a cube that is present in the other eye;
- depth outlines and authored stereo perspective remain intact.

The two eyes should be synchronized, not made pixel-identical. Pixel-identical
eyes would remove correct stereo perspective. Capture separate left/right eye
video if a mismatch remains.

## Test 5 — Beat Saber pause menu

1. Start Aether or 42 Flux from beat zero.
2. Pause during a visibly moving background section for 15 seconds.
3. Confirm the scene is frozen while the pause menu is open.
4. Press Continue.
5. Confirm the background resumes at the music/cube time without a jump ahead.
6. Repeat the pause twice in one run.

## Test 6 — Quest system menu, focus and sleep

1. During a moving scene, open the Quest system menu for 30 seconds and return.
2. Confirm no visual time jump.
3. Enter sleep for 3–5 minutes and wake the headset.
4. Confirm music, cubes, video, particles, animators and secondary cameras resume
   from one song clock.
5. Repeat with the Beat Saber pause menu already open before entering Quest
   Home. Returning to the app must remain paused until Continue is pressed.

## Test 7 — practice Custom Time

Start Aether and 42 Flux at several mid-song Custom Time positions, including a
time just before a scene boundary. Confirm catch-up reconstructs the current
scene without replaying old destroyed prefabs and without producing a permanent
black/grey background.

## Test 8 — BeatLeader and ScoreSaber upload eligibility

Run this test with only one leaderboard mod enabled at a time.

1. Complete a Vivify map with a valid score.
2. Confirm the leaderboard mod enters its normal posting/upload path rather
   than only logging a local replay save.
3. Confirm the result appears on the intended account/server.
4. Repeat a map containing fake no-score objects and confirm the replay remains
   accepted and fake misses are absent from scoring events.

An upload can still be refused by another MetaCore disabler, unsupported
gameplay modifiers, authentication, network state or server validation. Record
the exact reason from the leaderboard log; do not disable those protections.

## Test 9 — Quest 3 performance

Capture a stable 60-second 42 Flux section, then a second capture containing
pause/Continue:

```sh
pwsh -NoProfile -File scripts/collect-quest3-performance.ps1 -durationSeconds 60 -label vivify-0.6.5-flux
pwsh -NoProfile -File scripts/collect-quest3-performance.ps1 -durationSeconds 60 -label vivify-0.6.5-flux-pause
```

Check frame timing, thermal state, memory growth, repeated render-texture
allocation, per-eye errors and GPU stalls. A single visual pause authored by the
map is not by itself a performance failure; correlate it with logs and frame
timing.

## Pass criteria

A release pass requires all tests above on hardware with no new crash, no
repeated runtime exception, no fake-note scoring corruption, no background
advance during pause, no permanent one-eye loss and no cube occlusion at the
eclipse. Record the installed `.so` hashes with the results.
