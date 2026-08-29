# Vivify Quest 0.5.29 — Flux depth, frame pacing, and menu-exit crash fix

## Flux secondary color/depth cameras

- Multiview and Single-Pass Instanced captures now always use the shared
  stereo slot. Unity reported `stereoActiveEye=Left` even though Flux's source
  was a two-slice `Tex2DArray`; storing that array as a single-eye capture made
  later binding depend on an unrelated main-camera eye value.
- Secondary color and depth now retain the same texture-array layout. A mono
  Unity depth source is copied into both array slices so the map's
  `sampler2DArray` depth shaders receive a compatible texture rather than zero
  samples.
- Captured color/depth use `Graphics.CopyTexture` rather than allocating and
  executing an additional fullscreen CommandBuffer Blit every helper-camera
  frame.
- Debug logs record both the source and output depth dimensions, VR usage,
  texture dimension, and slice count for the first three frames of each
  secondary camera.

These changes target the black/partial NotesCam sections, playable cubes being
hidden by the Flux eclipse, and cubes disappearing when the main camera
blacklists them for secondary-camera compositing.

## Frame pacing

- Reapplying identical culling data is now a no-op. Previously the main camera
  cleared its renderer cache and diagnostic limits every Update, then rescanned
  up to thousands of renderers in `OnPreCull`.
- Clearing an already-empty culling controller is also a no-op.
- Existing 60-frame hierarchy refreshes still detect pooled/dynamic note
  renderers without paying the full traversal cost every frame.

The supplied Flux log contained 1,558 culling diagnostics and 588 measured
3 ms-or-longer culling stalls because this cache was reset continuously.

## End-of-level and menu stability

- A live beatmap state detected in the active `MainMenu` scene now receives
  the existing pointer-free transition reset before any further authored
  effects execute.
- Gameplay render components are never attached to `MenuMainCamera`.
- CameraApplier render callbacks pass the frame through on MenuMainCamera even
  if Unity switches cameras between two Runtime updates.

The captured 0.5.28 crash occurred after the result screen briefly appeared:
Unity called `CommandBuffer.Blit` with a gameplay Material after the gameplay
scene had ended. These guards remove that stale-material path.

## Murder Plot boundary

Murder Plot's data contains time-offset fake-note sequences, but the visible
horizontal rows are not authored: the dissolve point definitions are supposed
to cut out the copies so one cube is presented at a time. The Android Vivify
Blit shaders in the bundle include valid Multiview variants and do not generate
the rows. This release does not silently change another mod's note-animation
semantics; collect a Murder Plot run with Noodle logging for a separate
dependency fix if the rows remain.

## Validation boundary

ARM64 compilation and QMOD inspection prove source/package consistency only.
Complete `STABILITY_TESTS_0.5.29.md` on the Quest before broad distribution.
