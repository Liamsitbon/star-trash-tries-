# Vivify Quest 0.6.2 Aether headset acceptance matrix

Use the exact candidate hash from `BUILD_AND_TEST_0.6.2.md`. Keep debug logging
enabled for correctness runs, but disable it before performance comparisons;
the beat-zero Aether catch-up log is intentionally verbose and can itself
create a short debug-only frame drop.

## Preflight

- Confirm Beat Saber reports `1.40.8_7379`.
- Confirm Vivify reports `0.6.2`.
- Confirm both the staged and app-private `libVivify.so` hashes are:
  `a971414a686f8f7498958edb1c5288b5912ac5bc24172d2f61385b127d31926c`.
- Start a new logcat capture and preserve `Vivify.log` after each failure.
- Do not mix a 42 flux result into the Aether acceptance result; its secondary
  depth-camera issue is a separate renderer path.

## Test 1 — normal Play timing

1. Start Aether with the ordinary Play button from the beginning.
2. Record the headset view from before 0:38 through after 0:43.
3. Confirm notes/cubes remain aligned to the music.
4. Confirm the background does not jump over the drop opening and the expected
   scene appears at the reported recording position near 0:41–0:42.
5. Continue through the late ambient section and confirm it is not
   unexpectedly black/gray around the previously reported 1:55 position.

Pass requires correct visual content and timing, not merely absence of a
native crash.

## Test 2 — Custom Time reconstruction

1. Leave the level and re-enter Aether through Custom Time shortly before the
   late ambient section.
2. Confirm the active prefab begins at the elapsed position inside its event;
   it must neither restart from its opening nor jump to the clip end.
3. Repeat once from a time shortly after the beat-77 drop begins.
4. Inspect the `Vivify sync: registered` lines. `initialElapsed` should be near
   zero during normal Play and equal approximately
   `songTime - eventStart` during catch-up.

## Test 3 — short pause/resume

1. Start Aether normally and pause while a background prefab is active.
2. Resume after 15–30 seconds.
3. Confirm music, notes, background animation, particles, and video (if active)
   all continue at one clock.
4. Confirm the log reaches every resume checkpoint and ends with
   `Vivify resume complete`.

## Test 4 — Android sleep/resume reproduction

1. Start Aether normally.
2. Open the pause menu and let the Quest sleep for at least 5 minutes.
3. Wake the headset, return to Beat Saber, and close the pause menu.
4. Continue for at least 30 seconds.
5. Repeat the sleep/resume cycle once more in the same level.

Pass requires:

- no frozen background frame while audio continues;
- no black compositor output;
- fresh VrApi counters/frames after every pause-menu close;
- a `Vivify resume complete` line after every resume;
- no Unity pause/detach timeout attributable to a permanently blocked main
  thread after the resume;
- no native crash or Android ANR.

If it fails, preserve the last Vivify resume checkpoint, 10 seconds of VrApi
stats before/after it, `Vivify.log`, the Paper log, and a full diagnostics ZIP.

## Test 5 — release-performance sanity

After tests 1–4 pass, set `vivifyDebugLogging` to false, restart Beat Saber, and
capture at least 60 seconds of the same Aether segment with
`scripts/collect-quest3-performance.ps1`. Record FPS/stale-frame distribution,
CPU/GPU frame time, PSS/GPU allocation, temperature, and thermal clocks. This
is a performance acceptance step; it does not replace the visual tests above.

