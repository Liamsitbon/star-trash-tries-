# Vivify Quest 0.5.27 — native Multiview stability

## Stereo and post-processing

- Removed the Multiview Safety UI, runtime switch, filter, per-eye compositor,
  mono slice extraction, and copy-back path.
- `MULTIPASS_ENABLED` and `_StereoActiveEye` now run only in Unity's true
  MultiPass mode (`stereoRenderingMode == 0`).
- Single-Pass Instanced/Multiview screen and camera textures are explicit
  two-slice `Tex2DArray` RenderTextures with `VRTextureUsage.TwoEyes`.
- Stereo texture clearing now targets every array slice.
- Restored the six authored mid-render orders instead of remapping them to
  `AfterMainEffect`.
- Removed the unused secondary-camera per-eye `OnRenderImage` copy cache.

These changes target the repeated horizontal rows in Murder Plot, the
divergent/frozen eye output in 42 Flux and Aether, and the eye-locked image
reported after the eclipse section.

## Stability

- Removed the invalid per-frame `FindObjectOfType<BeatmapCallbacksController>`
  probe. The managed controller is now captured only from a real custom-event
  callback and retained only for same-scene practice reconstruction.
- Error logging no longer calls back into song-time acquisition while handling
  an exception.
- A pause-menu restart near time zero uses pointer-free transition cleanup and
  delays proactive probing of the outgoing scene.
- Missing asset warnings are deduplicated to prevent thousands of identical
  log writes during note spawning.

## Notes, shaders, and geometry

- Asset resolution supports exact path, Unity name, unique filename, and
  unique stem while rejecting collisions.
- Missing custom note/debris prefabs keep the stock gameplay renderer visible.
- Shaders reported unsupported by Unity receive a visible prefab fallback even
  when the material still reports compiled passes. Generic fallbacks remain
  prohibited for fullscreen Blits.
- Replaced the unfinished geometry HLSL string rewriter with a fail-closed
  public header/API that leaves source and meshes unchanged.

## Sabers and overlays

- Custom saber models now attach to the stable Saber root.
- Added a bounded 300-frame retry window and periodic replacement integrity
  checks for late base-game/CustomSaberAPI hierarchy rebuilds.
- Stock saber renderers are hidden only after a live replacement renderer was
  created.
- Beatmap preparation now suppresses AlwaysVisibleQuad instances that were
  already enabled and restores their original positions during a live reset.

## Validation boundary

The ARM64 library and QMOD can be built and structurally validated without a
headset. Visual parity, comfort, restart survival, and long-session stability
must still be confirmed on a physical Quest using `STABILITY_TESTS_0.5.27.md`.
