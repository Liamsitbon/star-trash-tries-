# Vivify Quest 0.5.28 — per-eye cameras and lifecycle stability

## Secondary cameras and the Flux eclipse

- Secondary cameras no longer use `Camera.targetTexture` or
  `SetTargetBuffers`; both paths disable stereo rendering on Quest.
- Each helper camera now stays on Unity's normal stereo render path and
  captures the source passed to `OnRenderImage`.
- Multiview stores the native two-slice texture array. True MultiPass stores
  Left and Right captures separately. The main camera binds the matching eye.
- Secondary depth properties copy Unity's camera-generated depth texture for
  the same captured eye before the main camera can reuse and overwrite it.
- Camera color/depth outputs are allocated lazily from the actual source
  descriptors and released with the authored camera event. They are no longer
  attached as camera targets that disable stereo.

These changes target Flux's eclipse hiding playable notes, the black/missing
NotesCam section, frozen eye-locked images, repeated rows, and divergent eye
output in camera-composited maps.

## Sabers

- Same-scene runtime preparation now preserves live SaberModelController/Saber
  targets and purges only invalid entries.
- Beat-0 `AssignObjectPrefab` events can therefore replace both sabers on the
  first map start; a pause or full Beat Saber restart is no longer required to
  repopulate the target list.

## Frame pacing

- Culling cameras cache renderer hierarchies per tracked root for 60 frames.
- Per-camera passes now move only enabled renderers on active GameObjects.
- Reused sets avoid allocating a new renderer-deduplication table each render.
- Object-prefab assignment lookup is indexed by object type and track. Maps
  such as Aether no longer linearly scan all 1,363 beat-zero assignments for
  every spawned note or debris object.
- Debug logging reports tracked roots, cached renderers, refreshed roots, and
  active layer moves so expensive maps can be measured from `Vivify.log`.

## Stereo diagnostics

- The first rendered use of each Blit material records the active XR mode,
  eye, source texture dimension/slice count, and Unity/material stereo keyword
  state. This distinguishes a missing Multiview shader variant from a runtime
  keyword or texture-layout failure without changing Unity's XR mode.

## Menu-exit crash

- Removed the obsolete global `ImageEffectController.OnRenderImage` hook. A
  captured Quest tombstone resolved its last callback to
  `Runtime::ShouldBypassImageEffect`, where it dereferenced a CameraApplier
  whose Unity object had already been destroyed during menu exit.
- CameraApplier's own transition guard remains responsible for its final
  callback and safely passes the source through when no beatmap is live.

## Validation boundary

The ARM64 build and package checks can prove binary consistency, not VR visual
parity or long-session stability. Complete `STABILITY_TESTS_0.5.28.md` on the
release Quest and retain the diagnostic ZIP before distributing broadly. See
`AETHER_ANALYSIS_0.5.28.md` for the bundle/event evidence behind the Aether
workload and stereo-diagnostic changes.
