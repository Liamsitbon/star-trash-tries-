# Nexora 2.0 Alpha

[Nexora 0.3.5 Direct Video remains recommended](https://github.com/Liamsitbon/star-trash-tries-/releases/tag/nexora-quest-0.3.5-direct-video-rc1).
Experimental branch: `nexora-2-alpha`. Target: Quest standalone Beat Saber
**1.40.8_7379**. Same `nexora` mod ID: replacement, never co-installation.
Reinstall the complete 0.3.5 QMOD to roll back library AND shader bundle.
No maps are published and no Alpha is installed automatically.

## Implemented prototype

- Selection of a V2/V3 difficulty prepares its beat-zero videos in the menu.
  Play waits for a **decoded frame**, not just Prepare completion. The paused
  decoder, texture and projection survive loading and matching LoadVideo events
  adopt them without Stop/Prepare again. Failed required media blocks startup.
- Targeted shader-variant collection is bundled. WarmUp availability is logged;
  omitted game APIs are not invoked through guessed native addresses.
- Existing RGB-only mono/SBS/OU maps retain their event/rendering path.
- Optional packed RGBD uses **one decoder and one texture**, not one per eye.
  Every mesh region samples its own animated radial depth on the GPU. Head X/Y/Z
  translation therefore changes near and far disparity differently. Rotation
  uses normal Unity stereo projection. Root motion is independent of depth.
- Per-map strength/world scale/ranges and low/balanced/high grids. Mesh allocation
  happens at load/quality change, not every frame. Balanced/high adds a local
  depth-edge check, blending badly stretched regions toward rotation-only RGB.
  It does not invent hidden-surface details. Low omits the extra fragment work.
- Capture poses use decoded **frame index / FPS**, not elapsed wall time. Packed
  RGB and depth cannot drift independently during pause, resume or seek.
- Missing/invalid packed-depth metadata or decoder errors request one RGB-only
  fallback on Update, outside native callbacks. Never two simultaneous decoders
  or endless retries. RGB failure remains an explicit startup error.

## Format for map authors and a future Vivify converter

Example `Nexora.LoadVideo` data (all files inside the map):

```json
{
  "id": "main", "media": "world-rgb.mp4", "projection": "mono",
  "autoplay": true, "syncToSong": true, "followPlayer": false,
  "rgbd": {
    "media": "world-rgbd.mp4", "layout": "color-left-depth-right",
    "encoding": "linear-radial-meters-code",
    "colorWidth": 0.8, "nearMeters": 2, "farMeters": 50,
    "strength": 1, "worldScale": 1, "quality": "balanced",
    "fps": 60, "durationSeconds": 180,
    "capturePoses": [
      {"time": 0, "position": [0,1.6,0], "rotation": [0,0,0,1]},
      {"time": 10, "position": [0,1.6,1], "rotation": [0,0,0,1]}
    ]
  }
}
```

Omit `rgbd` for ordinary RGB maps. RGB/depth panels cover the same mono 360
equirectangular directions, not separate eyes. Depth code 0=near and 1=far,
**radial metres**, not planar camera Z, inverse depth or arbitrary brightness.
Poses: capture-world metres, normalized XYZW quaternions, media-second timestamps.
`strength=0` cancels centre-head positional disparity; root animation is separate.
Source range: 2 <= near < far <=500m. World scale 0.1–4, with effective near >=0.5m
and far <=500m. Strength 0–1. RGBD cannot follow the head as a root transform.

`scripts/pack_rgbd.py RGB.mp4 DEPTH.mp4 OUTPUT.mp4 --near 2 --far 50`
accepts separate authored videos, checks projection/FPS/duration/frame count and
packs one H.264 stream. It does **not** estimate depth or render Vivify maps.
Keep the independent RGB file for fallback. The generated sidecar is LoadVideo
data to insert into the DAT; it is not an automatically discovered manifest.
Runtime separate RGB/depth decoders are not implemented. Packed stereo RGBD is
reserved for future view-layout extensions without changing timing/root contracts.
8-bit lossy video depth limits precision. Keep packed dimensions <=4096x2048 in
this prototype; actual decoder metadata and failures are logged.

## Evidence, performance and remaining work

Local ARM64 build, host tests and Android bundle rebuild passed. The bundle has
GLES3/Vulkan RGB/RGBD and retained Multiview variants.

**Actual Metal Editor GPU A/B:** same root transform, 0.15m camera translation:
near displacement 9.5px, far 2.5px; strength=0 displacement 0px. RGB regression
and near/far depth-occlusion checks passed. This is **not Quest stereo proof**.

ADB was disconnected during this build. Quest startup/practice/restart,
pause/resume/seek, per-eye rendering, fallback, memory pressure and sustained
CPU/GPU performance remain unverified. Do not promote Alpha until tested.

120Hz has an approximately 8.33ms frame budget; no FPS guarantee or forced refresh
rate/clocks. Grids: low 48x96, balanced 72x144, high 96x192. A 4096x2048 ARGB32
target costs 32MiB **excluding decoder buffers**. Debug logging records 240-sample
p95 Nexora Update CPU time and Unity frame interval, explicitly not app GPU time.
`scripts/collect_nexora2_metrics.sh NEW_DIRECTORY` collects existing logs and
Beat Saber memory without changing the app. Match with OVR Metrics app CPU/GPU
timings/stale frames for the same section on RGB and each quality level.

Limits: regional depth is not full scene reconstruction. Unseen surfaces cannot
be recovered; fast depth edges/large translations can ghost or smear. Future
layered/background captures are needed for better disocclusion. Startup prewarm
does not cover V4 or later-in-song loads yet. Authored black frames/fades are not
removed. Practice-start target-frame readiness needs a separate runtime test.
Once measured regressions and headset gates pass, this branch should replace
main's Nexora, not become a permanently separate mod.

## Primary references

- [Unity Prepare](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/Video.VideoPlayer.Prepare.html)
- [Unity stereo shader requirements](https://docs.unity3d.com/2021.3/Documentation/Manual/SinglePassInstancing.html)
- [Meta OVR Metrics](https://developers.meta.com/horizon/documentation/unity/ts-ovrmetricstool/)
