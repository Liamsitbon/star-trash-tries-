# Nexora — synchronised 360° environments for Beat Saber Quest

Nexora is an unofficial standalone Meta Quest mod by **Liam Sitbon**. It displays
pre-rendered cinematic environments around the player while Beat Saber's notes,
sabers and gameplay remain live. It is not a separate game or a PC mod.

This overview is intentionally release-independent. Check the selected QMOD's
manifest and [release notes](https://github.com/Liamsitbon/star-trash-tries-/releases)
for game compatibility, dependencies and feature availability. It does not imply
that every historical package supports every capability described here.

## What it does

- Projects equirectangular video onto a runtime-created inward-facing dome.
- Supports mono and packed over-under/side-by-side stereo RGB layouts.
- Uses the song clock for playback offsets, drift correction, pause/resume,
  restart, practice speed and seeking.
- Provides media preparation, layers, opacity transitions, dome transforms and
  supported deformation effects, within configured resource limits.
- Offers optional user-selected external video playback with an installed song.
- Includes an experimental, opt-in packed RGBD path for authored depth content.

Nexora alone does not load Vivify scenery bundles or convert arbitrary maps.
A hybrid map may keep Vivify effects and interactive objects live; it still
needs those mods and their compatible assets. Ordinary RGB maps need no depth.

## Rendering modes and limitations

The **Direct Video** path bypasses Nexora's fragment colour/UV effects:
brightness, RGB tint, exposure, hue, saturation, ripple, fog, scanlines,
vignette, pixelation, chromatic split, kaleidoscope and shader camera effects.
Opacity/fades, RGB projection, orientation, decoder/song timing, dome transforms
and applicable vertex deformation remain separate from this bypass.

The event API includes these effects, but recognising an event is not proof
that the selected renderer applies it. Do not disable a working rendering path
without an A/B test. Independent Vivify effects, notes and sabers are not
disabled by Nexora's Direct Video setting. Rendering workarounds are not a claim
that every shader/device issue has been resolved.

## Playing an authored map

Install one compatible Nexora QMOD and its declared dependencies, then install
a map containing Nexora media and events. No Nexora sphere prefab is required.
Adding an MP4 alone does not author those events.

Keep media at the map root when the map importer does not preserve subfolders:

```text
Info.dat
ExpertPlusStandard.dat
song.egg
cover.png
world-360.mp4
```

Add `Nexora` to the difficulty's requirements in `Info.dat`, preserving other
requirements. A minimal event inside `customData.customEvents` is:

```json
{
  "b": 0,
  "t": "Nexora.LoadVideo",
  "d": {
    "id": "world",
    "media": "world-360.mp4",
    "projection": "mono",
    "autoplay": true,
    "syncToSong": true
  }
}
```

Authored media paths stay inside the selected custom-map directory. Absolute
paths, parent traversal, missing files and remote URLs are not accepted via
map JSON. Explicit local-user selection is a separate path.

See [event reference](docs/EVENT_REFERENCE.md) and
[map contract](docs/NEXORA_CONTRACT.md) for loading, playback, animation,
transition, camera-effect and retirement events. Keep the authored timeline and
test decoder preparation before relying on a frame being visible at song start.

## Projection and orientation

| Layout | Accepted names | Typical full-frame ratio |
| --- | --- | --- |
| Mono panorama | `mono` | 2:1 |
| Two panoramas stacked vertically | `topBottom`, `overUnder`, `tb`, `ou` | 1:1 |
| Two panoramas side by side | `sideBySide`, `sbs` | 4:1 |

Yaw/pitch/roll rotate the environment. `flipX`/`flipY` mirror image axes and
`swapEyes` exchanges stereo views. Use heading/yaw to correct a world facing
behind the player; mirroring or eye swapping is not the same operation.

## Optional external-video menu

Open **Nexora** from the game's mod controls; the precise menu location depends
on the installed build. Select a readable video and an installed song, adjust
the available offset/orientation controls, then choose **Custom: use in place +
open song**. Select a difficulty and press the game's normal **Play** button.

- The video plays from its existing location. Selecting it does not import,
  copy, move, rename, delete or modify it, or rewrite the map's DAT files.
- The binding is session-local and specific to the selected song.
- **Normal map mode** clears the override and restores authored map behaviour.
- **Copy to map** is a separate confirmed action for writable custom maps. It
  retains the source and does not overwrite an existing destination. Copying
  alone does not generate Nexora events.
- Storage permissions, ownership and normal difficulty requirements still
  apply. A listed file extension is not a codec compatibility guarantee.

See [external-video contract](docs/EXTERNAL_VIDEO.md).

## Optional RGBD

RGBD requires real, time-matched colour and **radial distance** data. The packed
layout uses colour on the left and grayscale depth on the right of one frame,
with one decoder and clock. These panels are not the two eyes of stereo video.

Depth reconstructs a world-anchored surface for positional parallax; moving or
scaling the whole dome is not equivalent. The author configures layout,
near/far distance, capture origin and supported strength/world-scale/quality
settings. Ordinary RGB media does not gain depth automatically.

This is experimental **2.5D**, not the complete original scene. Hidden surfaces,
depth quantisation, finite mesh sampling, compression and disocclusion limit
the result. The packed path requires mono projection; RGB stereo support does
not imply stereo RGBD. A configured RGB fallback still requires valid media.
There is no promise of identical Vivify appearance or zero additional GPU cost.

## Authoring and conversion

Render the environment with its intended timing and capture orientation, encode
compatible 360° media, add map events, then test on the target headset.
NexoraCapture is an experimental Unity authoring tool for supported timelines
and matched RGB/depth capture, **not a complete Vivify or Beat Saber simulator**.
Unsupported shader, camera, animator and event behaviour needs adapters or live
content. A placeholder-shader export is not a faithful conversion.

Keep interactive notes, sabers and cut-dependent effects live. Preserve rights
and credits for music, video, map assets and shaders; the mod's software licence
does not grant permission to redistribute that content.

## Performance and troubleshooting

Video FPS is not headset refresh rate. High game FPS does not prove support for
an equally high-FPS video. Codec/profile, resolution, bitrate, stereo, layer
count, RGBD and other live mods all affect decode, bandwidth, memory and timing.
Measure the actual map on the intended device; no sustained FPS is guaranteed.

For black/white output, separate: inactive map events; missing or unsupported
media; decoder failure; and a valid decoded frame not reaching the intended
renderer. For missing effects, check the rendering-path limitation above.
For parallax artifacts, inspect depth encoding, matched capture frames and
origin. Preloading reduces avoidable waits but cannot make every seek instant.

Test startup, transitions, both eyes, pause/resume, practice seek and scene exit.
Source tests, an ARM64 build and a clean QMOD are not headset runtime proof.

## Build and package

Use the toolchain and target settings declared in the checked-out source and
package manifests. The existing build entry points are:

```bash
qpm restore
./scripts/build.sh
./scripts/test_host.sh
python3 ./scripts/validate_contract.py
./scripts/build_assets.sh
python3 ./scripts/package_qmod.py
python3 ./scripts/package_source.py
```

Changing Unity shader/material assets requires rebuilding the Android asset
bundle before packaging. Outputs belong in `release/`; do not mix binaries from
different source revisions. Do not add maps or macOS metadata to QMODs.

## Credits and licence

**Liam Sitbon** created and maintains Nexora. Nexora's own source uses the
[MIT License](LICENSE); preserve its notices. Dependencies and third-party
assets retain their own licences and credits.

Nexora is not affiliated with or endorsed by Beat Games or Meta.
