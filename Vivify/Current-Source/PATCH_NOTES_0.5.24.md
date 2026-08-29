# Vivify Quest 0.5.24 — transition cleanup and transient Multiview effects

## Native transition-crash fixes

- Level selection drops raw gameplay-scene pointers before touching the previous AssetBundle.
- `AssetBundle::Unload(false)` replaces `Unload(true)` so loaded Unity objects are not invalidated underneath cleanup code.
- Scene-transition reset never probes or destroys stale cameras, renderers, prefabs, behaviours, or RenderTextures.
- Persistent RenderTextures use Unity's deferred `Object::Destroy` path; Vivify no longer calls `RenderTexture::Release()` before destruction.
- CameraApplier rejects late callbacks after transition reset and never performs a source-equals-destination self-Blit.
- Global camera/screen texture bindings are nulled before references are forgotten.

These changes target the supplied tombstones in `RenderTexture::Release`, `Object::Destroy`, `Behaviour.enabled`, and `Renderer.enabled` after leaving a Vivify song and selecting another song.

## 42 Flux visual recovery

- Multiview safety no longer disables every camera effect.
- Safe direct `_Main -> _Main` effects run inside `OnRenderImage` through frame-local `Graphics.Blit` textures.
- The authored 42 Flux Bloom downsample/upsample graph is rebuilt from mono frame-local declared textures; no persistent TwoEyes RenderTexture survives the frame.
- Whitelist isolation for mono NotesCam/depth cameras is restored, so camera textures contain only the authored tracked objects instead of the entire scene.
- Mono secondary-camera main effects remain available while Multiview safety is enabled.

## Murder Plot protection

- Known unstable direct shaders remain blocked in Multiview safety: GaussianBlur, ChromaticAberration, KickWow, Vignette, PixelSort, Scram, Difference, MotionBlur, and ColorZoom.
- `AssignObjectPrefab.hideOriginal` is independent of Multiview safety again. A live replacement hides the original note, preventing duplicate/striped note rendering; the original remains only when the replacement has no live renderer.

## Still intentionally disabled in Multiview safety

- Mid-render CommandBuffers and unknown declared-texture graphs.
- Desktop-authored shader chains that are not on the verified direct/Bloom transient paths.

Disabling **Multiview safety** still restores the unrestricted reference behavior.
