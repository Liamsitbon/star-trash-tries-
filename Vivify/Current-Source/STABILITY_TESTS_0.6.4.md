# Vivify Quest 0.6.4 per-eye acceptance matrix

Use the exact 0.6.4 hashes recorded in `BUILD_AND_TEST_0.6.4.md`. Test with
Vivify debug logging enabled for the correctness pass.

## Test 1 — same authored content in both eyes

1. Start a Vivify map normally from the beginning.
2. Enter a section that creates a secondary camera, such as the 42 Flux
   `NotesOneCam` or `NotesCam` sections.
3. Close one eye at a time without moving the headset.
4. Confirm the same objects and effects exist in both eyes.
5. Confirm their small viewpoint/parallax difference remains natural; a flat
   duplicated image is a failure.
6. Confirm notes remain readable and no black moon/foreground element hides
   notes in only one eye.

The log must contain `secondary stereo sync` for each created camera, with
`mode=3`, `perEyeMatrices=true`, and `xrManagedCulling=true`. Any `secondary
stereo sync failed` warning is a failure.

## Test 2 — two-slice capture integrity

For every created secondary camera, confirm the first capture diagnostics say:

- `vrUsage=2`
- `dimension=5`
- `slices=2`
- `color=true` when a color texture was authored
- `depth=true` when a depth texture was authored

Do not accept a mono texture duplicated into both eyes as the main color path.
The duplication fallback is allowed only when Unity exposes a genuinely mono
auxiliary source, especially depth.

## Test 3 — Aether regression

Play Aether from the beginning through the cube-pause/prefab-switch bridge.
Confirm the 0.6.3 result remains fixed: no un-authored stall, correct prefab
switching, and continued music/background synchronization.

## Test 4 — lifecycle and XR resume

1. Pause and resume during a secondary-camera section.
2. Put the Quest to sleep for several minutes, wake it, and continue.
3. Restart the map from the pause menu.
4. Exit to Main Menu, wait ten seconds, then start another Vivify map.
5. Confirm both eyes stay consistent and there are no stale camera, culling,
   texture or native `Blit` failures.

## Test 5 — performance

Capture at least 60 seconds across the same map section with:

```sh
pwsh -NoProfile -File scripts/collect-quest3-performance.ps1 -durationSeconds 60 -label vivify-0.6.4-per-eye
```

The matrix work is four small camera API updates per secondary camera frame;
it must not add a full-screen copy, a scene-wide search, or an extra render
pass. Preserve video and logs because average FPS cannot prove per-eye visual
correctness.
