# Vivify Quest 0.6.3 Aether note-stall acceptance matrix

Use the exact candidate hashes recorded in `BUILD_AND_TEST_0.6.3.md`. Do not
mix a 42 flux result into this acceptance run.

## Preflight

- Confirm Beat Saber reports `1.40.8_7379` and Vivify reports `0.6.3` in a new
  process.
- Preserve `Vivify.log`, the Paper log, Android logcat, and the headset video.
- Enable Vivify debug logging for the correctness pass. Disable it only for
  the final A/B performance capture.
- Start Aether with ordinary Play from the beginning; do not use Custom Time
  for the primary stall reproduction.

## Test 1 — Aether bridge pause scene

1. Play continuously through the intentional cube-pause/wireframe scene.
2. Confirm cube motion still pauses and resumes in time with the authored
   lights and music.
3. Confirm the headset does not freeze for an un-authored moment when the note
   prefabs switch.
4. Confirm gemstone and wireframe replacements still render and that the
   original Beat Saber note is not doubled underneath them.
5. In the performance heartbeat, record `noteRefreshEvents`,
   `noteRefreshCandidates`, and `prefabLookupRebuilds` for the interval.

Pass requires both correct visuals and absence of the reported discrete stall.
A high average FPS alone is not a pass.

## Test 2 — first assignment after note spawn

Run a Vivify map or controlled chart that assigns a prefab to an already
active note which did not previously have a custom replacement. Confirm the
note changes immediately. This protects the compatibility behavior that would
be lost by copying Axo's original replacement-map-only loop literally.

## Test 3 — pooling and restart

1. Restart Aether from the pause menu during the bridge.
2. Play through the same scene again.
3. Confirm pooled notes receive the correct current prefab and no stale
   replacement survives from the first run.
4. Leave to Main Menu and start the map again in a new gameplay scene.
5. Confirm there is no stale-controller crash or invisible original note.

## Test 4 — existing 0.6.2 regressions

Repeat the normal-Play timing, Custom Time reconstruction, short pause/resume,
and Android sleep/resume tests from `STABILITY_TESTS_0.6.2.md`. The note-stall
change must not regress callback-clock timing or XR resume handling.

## Test 5 — release-performance A/B

With debug logging disabled, capture the same Aether bridge interval once with
0.6.2 and once with 0.6.3 using:

```sh
pwsh -NoProfile -File scripts/collect-quest3-performance.ps1 -durationSeconds 90 -label vivify-0.6.3-aether-bridge
```

Compare frame-time/stale-frame behavior, CPU, GPU busy percentage, clocks,
memory and thermal state. Preserve the video because a GPU utilization average
cannot prove that a one-frame or multi-frame stall is absent.
