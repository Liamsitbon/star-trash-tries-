# Nexora 2.0.0-rc.2 — external video + forward orientation · Quest 1.40.8_7379

New in RC2:

- Mod Settings → Nexora: choose an accessible video and an installed/loaded
  song, then open native Solo difficulty selection and Play. **Custom reads
  the file in place**: no import, move, copy or map-data rewrite.
- **Copy to map** is separate and requires explicit confirmation. It retains
  the source, exclusively creates a new destination, rejects overwrites, and
  removes its own cancelled/error partial output. Copying does not author DATs.
- **Normal map mode** clears the session-local override. Exact gameplay-level
  identity prevents the selected video leaking into a different song.
- Conventional equirectangular video centre now faces forward, not backward.
  Explicit `azimuthConvention` preserves U=0-forward captures; initial exact
  180-degree legacy yaw compensation is not doubled. Root/capture animation
  remains separate from this media-basis correction.
- Quest BSML 0.4.55 is a new menu dependency; SongCore remains 1.1.26.

Host tests exercise selection without writes, copy retention/no-overwrite,
invalid paths, cancellation and orientation math. Android ARM64 compilation
and package validation are separate from headset proof. The new menu, actual
storage access, per-eye playback and pause/seek need a Quest playtest.
See [menu details](../Nexora/Current-Source/docs/EXTERNAL_VIDEO.md).

## Retained unified RGB/RGBD baseline

The user's request to combine Nexora 1 and Nexora 2 supersedes the earlier
permanent-separate-Alpha recommendation. One package, `id: nexora`, now provides
RGB-only playback and opt-in RGBD. Old release tags remain available as history.
Do not install two nexora QMODs. No shader or decoder rewrite accompanies the
name/version unification; the Alpha Android asset and provenance stay identical.

The user reported visible video and 118–120 FPS, plus one level-selection crash
which did not recur on retry. They also reported no visible depth difference.
Those observations do NOT verify RGBD rendering/performance or resolve the crash.
The selection callback already catches C++ exceptions; that alone cannot prevent
a native/Unity crash. No fresh crash log is available while Quest is charging.

Depth stays optional and needs actual synchronized RGB/depth media, a `LoadVideo`
event with `rgbd`, and the separate RGB fallback file. Ordinary MP4s cannot gain
scene depth merely by upgrading the mod. A sidecar alone does not activate depth.
The default Direct Video path is preserved; its existing fragment-effects bypass
is not expanded. Live Vivify objects and effects remain independent.

NexoraCapture is a separate offline Unity authoring project. It records RGB and
actual render-target depth at identical frame-index times. It is not a complete
Beat Saber emulator. Unsupported capture adapters must be implemented, not
replaced with generic shaders or silently omitted. Transparent surfaces without
depth writes currently inherit the underlying opaque surface's depth.

Known limitations retained: V2/V3 beat-zero video prewarm (not all V4/practice
start paths), possible blank interval on decoder fallback, approximate hole
repair, lossy 8-bit depth in final packed H.264, no measured Quest RGBD GPU timing.
The release remains a candidate rather than an unsupported stability guarantee.
