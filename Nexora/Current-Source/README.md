# Nexora 0.3.1 — Beat Saber Quest

Nexora is a separate Quest mod for map-synchronised pre-rendered 360° worlds.
It is not Vivify and has no dependency on Synapse Server.

## Fixed architecture

- **Map ZIP:** 360° video, beatmap data, Nexora events and only live assets
  still required for gameplay, sabers, notes or interactive effects.
- **Nexora mod:** runtime-created inward-facing sphere/dome, video decoder,
  mono/stereo projection, song synchronization, deformation/effects and
  lifecycle logic.
- The map never ships a Nexora sphere prefab. `Nexora.LoadVideo` resolves a
  safe path inside the selected custom-map directory and creates/configures
  the runtime dome automatically.
- Absolute paths, `..`, missing files and remote URLs are rejected. Media is
  not read from a shared mod folder.

This preserves Nexora's purpose: expensive Vivify scenery is pre-rendered to
video, not rebuilt as full live 3D. Notes, sabers and selected interactive
Vivify camera/gameplay effects can remain live and separate.

## Runtime features

- Mono, over/under and side-by-side stereo equirectangular projection.
- Up to 1–6 layers (three by default), preloading and cross-fades.
- Song-time drift correction, practice speed, seek, pause/resume, restart,
  focus loss and safe scene retirement.
- Rotation, scale, offset, opacity, brightness, tint, exposure, hue,
  saturation, deformation, ripple, twist, pinch, pulse, fog, scanlines,
  vignette, pixelation, chromatic split, kaleidoscope and additive blending.
- Camera-effect events implemented in the Nexora dome shader itself. Quest never
  attaches a framebuffer `OnRenderImage` component, avoiding the known
  Single-Pass Multiview compositor-freeze path and keeping Nexora independent
  from Vivify's render pipeline.
- Unity owns Android video decoding and material binding inside Beat Saber's
  Vulkan renderer. A symmetric procedural safety backdrop remains visible
  until a real decoded `frameReady` callback replaces it, so maps that disable
  the stock environment do not collapse into a black world on decoder failure.

## Map example

Put the actual file at `Nexora/Media/mountains.mp4` inside the map ZIP and add
the same `mountains.mp4` at the map root for MBF compatibility. Add `Nexora`
to the difficulty requirements, and author:

```json
[
  {
    "b": 0,
    "t": "Nexora.LoadVideo",
    "d": {
      "id": "world",
      "media": "Nexora/Media/mountains.mp4",
      "radius": 90,
      "projection": "mono",
      "opacity": 0,
      "autoplay": true,
      "syncToSong": true
    }
  },
  {
    "b": 2,
    "t": "Nexora.AnimateDome",
    "d": {"id": "world", "opacity": 1, "durationSeconds": 3, "ease": "inOut"}
  }
]
```

Use the separate `Vivify-to-Nexora-Converter` project to embed media, preserve
required live events and generate these events automatically.

`projection` accepts `mono`, `topBottom`/`overUnder`/`tb`/`ou`, and
`sideBySide`/`sbs`. `flipX`, `flipY`, and `swapEyes` correct common exporter
orientation/eye-order mismatches without re-encoding. For geometrically
correct equirectangular packing, mono is 2:1, OU is 1:1 (two 2:1 eyes stacked),
and SBS is 4:1 (two 2:1 eyes side by side).

## Build and package

```bash
qpm restore
./scripts/build.sh
./scripts/test_host.sh
python3 ./scripts/validate_contract.py
./scripts/build_assets.sh
python3 ./scripts/package_qmod.py
python3 ./scripts/package_source.py
```

Output: `release/Nexora-Quest-0.3.1.qmod`. The package script requires a real
Android UnityFS shader bundle, matching Unity-source provenance, AArch64 ELF
shared object, an exact manifest/payload contract and a clean ZIP without PC or
macOS payloads.

Target: Beat Saber Quest `1.40.8_7379`, Scotland2, Unity `2021.3.16f1`, Android
NDK `27.3.13750724+preview-0`.

## Estimated performance — not a guarantee

These are starting estimates only; do not rely on them without testing the
exact headset, codec, bitrate, effects and map.

| Headset | Conservative start | Higher-quality experiment |
|---|---|---|
| Quest 2 | H.264 2048×1024, 30–60 fps | 2880×1440 at 30 fps |
| Quest Pro | H.264/HEVC 2880×1440, 60 fps | 3840×1920 at 30 fps |
| Quest 3 / 3S | HEVC 3840×1920, 30–60 fps | Higher decode sizes only after exact-device testing |

Nexora's media helper intentionally has no 90/120-fps preset. Meta's current
immersive-video guidance targets 48–60 fps and requires per-device codec
capability checks; Beat Saber also consumes GPU and decoder resources. If
frames drop, reduce camera effects, layer count, fps, bitrate, then resolution.

## Validation boundary

An ARM64 build and valid QMOD prove source/build/package consistency only.
Actual video decode, both-eye stereo, visual quality and sustained performance
must be verified in the exact Beat Saber map on the target Quest. See
`docs/EVENT_REFERENCE.md` and `docs/NEXORA_CONTRACT.md`.
