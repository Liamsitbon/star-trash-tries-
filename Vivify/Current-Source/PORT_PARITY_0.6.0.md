# Vivify Quest 0.6.0 port and parity record

## Source of truth

- Input archive: `<restored-workspace>/Vivify-master.zip`
- Windows project version: `1.1.0`
- ZIP snapshot/comment: `f83aa3c51290e288edae872dc2dac6b66112d3d6`
- Quest target: Beat Saber `1.40.8_7379`, Scotland2, Android ARM64
- Quest release version: `0.6.0`

The supplied Windows source—not the old Quest patch notes—is the behavioral
reference. Older statements that a bug was fixed are treated as unverified.
The existing Quest code is used only where a platform foundation is necessary:
QPM/Scotland2 entry points, IL2CPP bindings, Android bundles and XR/per-eye
texture handling.

## Custom-event coverage

| Windows-master event | Quest 0.6.0 implementation | Important parity details |
| --- | --- | --- |
| `InstantiatePrefab` | Source-covered | Asset/id lookup, transform, tracks, synchronization and left-handed mirrored parent |
| `DestroyObject` | Source-covered | Id removal, sync unregister and Unity object cleanup; camera property state remains keyed by id like desktop |
| `SetMaterialProperty` | Source-covered | Texture, color, vector, float, int and keyword handling; point-definition animation |
| `SetAnimatorProperty` | Source-covered | Bool, float, integer and trigger handling; point-definition animation |
| `SetGlobalProperty` | Source-covered | Global texture/color/vector/float/keyword state and animation |
| `Blit` | Source-covered | All eight orders, authored pass, priority, duration/easing, duplicate entries and deterministic equal-priority ordering |
| `CreateCamera` | Source-covered with Quest XR adaptation | Targetless stereo capture, color/depth texture arrays, BloomPrePass copy, main effect default-on, property-baseline restoration and desktop duplicate-id rejection |
| `CreateScreenTexture` | Source-covered with Quest XR adaptation | Explicit/ratio sizing, format/filter settings, stereo layout preservation and desktop duplicate-id rejection |
| `SetCameraProperty` | Source-covered | Depth mode is ORed with camera baseline; null restores depth/clear/background baseline; culling/Bloom/main-effect behavior matches master defaults |
| `SetRenderingSettings` | Source-covered | Render, Quality and XR settings; capture/restore; point animation; literal DynamicGI refresh with safe unavailable fallback |
| `AssignObjectPrefab` | Source-covered | Object/any-direction/debris/trail assignments, multi-track fan-out, live-note refresh and stock-object reveal semantics |

“Source-covered” means that parsing, state and execution paths exist and were
compared with the supplied Windows implementation. It is not a physical-device
result.

## Runtime and lifecycle foundation added for Quest

The old Quest project kept render state in a persistent singleton without a
strong ownership boundary. Version 0.6.0 adds a standalone lifecycle state
machine (`Dormant`, `Preparing`, `Active`, `Suspended`, `Retiring`) and a
monotonic session generation. Render callbacks, command buffers and camera
components must belong to the active generation. Scene teardown invalidates
the generation first and defers destructive cleanup if a render callback is
already on the stack.

This foundation is used by:

- beatmap preparation and catch-up reconstruction;
- pause/resume, retry and backward practice seeks;
- early gameplay-scene transition cleanup;
- late pointer-free cleanup after Unity has begun destroying scene objects;
- main-menu stale-session detection;
- `CameraApplier` and mid-render command-buffer ownership;
- secondary cameras, captured textures, materials and render-setting restore.

## Bugs found from code and evidence, not old claims

1. A matching 0.5.30 binary crashed natively after gameplay had already
   returned to Main Menu. The stack entered Unity's material resolver from
   `CommandBuffer.Blit`; symbolization led through `Runtime::ApplyBlits` and a
   stale `CameraApplier` attached/refreshed on `MenuMainCamera`. The new
   generation gate, early transition reset, menu guard and owner-specific
   command-buffer removal address that source path.
2. The old Quest renderer bypassed the registered `ImageEffectController`
   callback, which changed Windows-master ordering and could execute Beat
   Saber's effect twice. The callback is now routed exactly between authored
   before/after chains while the original hook is suppressed only for the
   active owned renderer.
3. Blits with identical material data were collapsed, even when the map
   authored two passes. Duplicate authored passes are preserved, and
   equal-priority ordering follows desktop insertion/reverse-render semantics.
4. Secondary cameras defaulted `mainEffect` to false on Quest although Windows
   resolves unset/null to true. Default-on plus explicit false is now used.
5. Secondary camera null properties did not restore their copied baseline, and
   authored depth modes replaced rather than extended it. Baselines are now
   cached; depth capture remains part of the Quest baseline.
6. Camera copies omitted BloomPrePass state. Renderer, effect container,
   render-data object and mode are copied from the main camera.
7. Video time used absolute song time rather than time since prefab creation.
   Only play-on-awake players are registered; target time is
   `max(0, songTime - prefabStartTime)` with pause/time-scale handling.
8. Object-prefab assignment updated only one track and did not refresh all
   live notes. Each track gets independent assignment state and active notes
   are refreshed. Null/single/additive stock-object reveal rules follow master.
9. Left-handed prefab mirroring and Windows saber-trail enable/disable ownership
   were absent. Both are implemented and saber hooks are scoped to an active
   Vivify beatmap so ordinary maps are not modified.
10. Arbitrary render-texture, trail and Blit caps rejected valid authored data.
    Validation now uses actual Unity/system limits and required lower bounds.
11. Camera/screen-texture ids were silently replaced, while desktop uses
    add-only declarations until `DestroyObject`. Duplicate declarations are now
    rejected. Destroying a camera removes the concrete camera but keeps its
    keyed `SetCameraProperty` state, so a later recreation receives the same
    state as Windows master.

## Quest-specific rendering implementation

- Single-Pass Multiview/Instanced paths preserve two-slice texture arrays.
- MultiPass retains per-eye slots and the `MULTIPASS_ENABLED` shader state.
- Secondary cameras keep `targetTexture` unset because assigning it disables
  stereo camera rendering on Quest; output is captured from `OnRenderImage`.
- Color/depth copies normalize shader-facing layout and never bind a 2D texture
  where the effect expects `sampler2DArray`.
- Beat-zero post-processing allocation remains frame-staggered to avoid loading
  GPU resources inside the most congested startup frame.

## Platform-specific non-ports

- Camera2 priority patches: Camera2 is a Windows camera-mod integration and has
  no Quest runtime component to patch.
- Desktop mirror hash indexing: the Windows mirror-mod integration has no Quest
  equivalent. Quest XR eye rendering is handled directly instead.
- The Windows project keeps AudioSource synchronization commented out; 0.6.0
  does not present that disabled code as a supported feature.
- Windows uses reusable replacement-prefab and saber-trail pools. The Quest
  port preserves the authored assignment/lifecycle behavior but currently
  instantiates and retires replacement objects through Unity instead of copying
  that desktop pool implementation. This is a performance implementation
  difference, so dense replacement maps require headset frame-time validation.

## Evidence boundary

Completed locally: source comparison, lifecycle unit test, Android ARM64
compilation and package inspection. Not completed locally: installation on a
Quest, real gameplay, per-eye visual inspection, GPU timing, or confirmation
that the historical crash no longer reproduces. Those require the matrix in
`STABILITY_TESTS_0.6.0.md` and device logs.
