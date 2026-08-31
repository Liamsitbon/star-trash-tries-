# Nexora product and Quest compatibility contract

Nexora is Liam Sitbon's standalone Beat Saber Quest mod for synchronized,
pre-rendered 360-degree cinematic environments. It targets Beat Saber
`1.40.8_7379` with Scotland2 and does not load, render, or depend on Vivify
asset bundles.

## Required behavior

- Load map-embedded 360 video and render it on runtime-created inward domes.
- Synchronize video to song time, practice speed, pause/resume, seek, restart,
  focus changes, and map/scene retirement.
- Support multiple layers, preload/cross-fade, visual events, mono, over-under,
  and side-by-side projection.
- Support local MP4 media through Unity's Android VideoPlayer integration,
  principally H.264 or HEVC. Unity owns the decoder surface and binds its
  texture through the same Vulkan renderer used by Beat Saber. Nexora does not
  bundle a desktop codec stack; codec/profile/level support is device-specific.
- Keep a symmetric procedural safety backdrop visible while a video prepares
  or after it fails. The authored video dome may replace it only after a real
  `frameReady` callback and a live Unity texture; a black placeholder is never
  treated as readiness.
- Apply camera-event visuals through the Quest Multiview-capable dome shader.
  Nexora must not attach the desktop-style `OnRenderImage` framebuffer path.

## Map package contract

Normal Beat Saber files remain in the map. The authored DAT must declare the
`Nexora` requirement and reference media through a `Nexora.LoadVideo` event.
The preferred file is:

```text
Song Folder/
  Info.dat
  ExpertPlusStandardNexora.dat
  song.ogg or song.egg
  cover.png
  Nexora/Media/world-360.mp4
  world-360.mp4                 # MBF root duplicate
```

The DAT path is normally `Nexora/Media/world-360.mp4`. Nexora first resolves
that exact case-sensitive map-relative path. If it is absent, it may use the
same-name root duplicate required by MBF. Absolute paths, Windows drive paths,
URLs, traversal, symlink escape, and files outside the selected map are
rejected.

## Projection and distortion contract

- `mono`: one 2:1 equirectangular eye.
- `ou` / `topBottom` / `overUnder`: two 2:1 eyes stacked, normally 1:1 total.
- `sbs` / `sideBySide`: two 2:1 eyes side by side, normally 4:1 total.
- `flipX`, `flipY`, and `swapEyes` are supported as numeric or Boolean DAT
  fields for exporter correction.

Packing a stereo source into the wrong total aspect ratio creates stretching;
`scripts/prepare_media.py --projection ...` preserves the selected geometry
and warns when its input aspect does not match.

## Proof boundary

Source validation, an Android UnityFS shader bundle, an AArch64 native build,
and a valid QMOD establish project/package consistency. They do not prove
decoder support, correct stereo in both eyes, sustained framerate, or lifecycle
stability on hardware. Those require the exact map and media on the target
Quest.

Nexora is MIT-licensed. Copyright Liam Sitbon.
