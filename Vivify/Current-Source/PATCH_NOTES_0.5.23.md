# Vivify Quest 0.5.23 — full stability pass

This build keeps every user-facing feature enabled by default while making the
Multiview safety switch a real isolation boundary.

## Rendering stability

- Multiview safety still bypasses incompatible fullscreen Blits and declared
  screen-texture chains, preventing the white Bloom scene in 42 Flux and the
  repeated rows/trails in Murder Plot.
- Safety now also prevents all temporary culling-layer mutation. A secondary
  camera can no longer move thousands of live note renderers to layer 22 while
  its composite is bypassed.
- The main camera is no longer intercepted by `CameraApplier` merely because a
  mono secondary camera exists in the safe path. This removes an unnecessary
  full-resolution pass-through and a source of black/white frames.
- Turning Multiview safety off remains complete: full Blits, screen textures,
  secondary-camera culling and authored effects are restored.

## Pause, seek and cleanup

- Secondary cameras remember their exact enabled state across pause/resume. A
  camera intentionally created disabled is no longer enabled into the headset
  after resuming.
- Culling layers are restored from `OnDisable`, `OnPostRender`, reset and camera
  destruction, covering scene changes where Unity skips a render callback.
- Mid-render command buffers are removed before pause.
- A backward time jump now rebuilds the beatmap proactively on the next frame
  from the current practice/seek time instead of waiting for a later event.
- The 0.5.22 live-prefab reconstruction and historical-prewarm filtering remain
  in place to prevent seek-time allocation bursts.
