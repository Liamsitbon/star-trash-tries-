# Unified Nexora 2.0.0-rc.1 — Quest 1.40.8_7379

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
