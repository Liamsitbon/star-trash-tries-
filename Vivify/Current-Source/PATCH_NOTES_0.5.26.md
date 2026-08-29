# Vivify Quest 0.5.26 — per-eye Multiview compositor

## Renderer

- Replaces the 0.5.25 shader denylist with a deterministic two-eye compositor.
- Extracts each `VRTextureUsage.TwoEyes` slice to a mono temporary texture.
- Runs the complete authored Blit graph independently for eye 0 and eye 1.
- Writes each completed frame back to the matching destination slice.
- Binds per-eye views of declared screen textures and secondary-camera color/depth textures while materials execute.
- Remaps unsupported mid-render Blits to `AfterMainEffect` instead of attaching texture-array CommandBuffers.

## Map fixes targeted

- 42 Flux: prevents one-eye/white-frame output while retaining Bloom, NotesCam, depth and eclipse composition.
- Murder Plot: prevents fullscreen effects from sampling the combined stereo array as a 2D image, which produced repeated rows of notes.
- Preserves the normal prefab/material path used successfully by You, Aether and Overclocked.

## Stability retained from 0.5.24–0.5.25

- Scene-transition reset does not probe stale Unity objects.
- Persistent RenderTextures are destroyed only through their owning runtime record.
- Temporary per-eye textures are released before `OnRenderImage` returns.
- Seek/prewarm recovery and pause camera cleanup remain enabled.

`Multiview safety` now means **use the per-eye compositor**. Turning it off restores the unmodified render path for comparison.


## Build compatibility correction

The source package now uses `Graphics::CopyTexture` for per-eye slice extraction and write-back. This matches the Beat Saber 1.40.8 bs-cordl bindings and fixes the two six-argument `Graphics::Blit` compilation errors.
