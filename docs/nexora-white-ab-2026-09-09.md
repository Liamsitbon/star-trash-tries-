# Nexora 0.3.4: real-device white-screen A/B

## Observed, not inferred

The user reported a white fade with Nexora 0.3.4 in normal mode. With the same
installed runtime and Android asset, setting `rawVideoDiagnostic=true` made the
actual video visible on Quest 3S. The user then confirmed the video was upright
but faced backwards, not vertically inverted.

The installed runtime SHA-256 was
`579ca97bca6f302e0b70503a8a35d662c88a5da9de7d6741f63624b0961186a9`.
The installed asset SHA-256 was
`a5314c8f89914b57c4b0b3af2ba04a5606dc8edf0ca0c1be95118c0d5bef920a`.

`diagnostics/nexora-0.3.4-white-2026-09-09/Nexora.raw-visible.log`
records GLES (`api=11`), frame 446 at 7.433 seconds, `raw=1`, `simple=1`,
`rgbd=false`, brightness 1, exposure 0, white tint and identity UV transform.
The decoder target, VideoPlayer target/internal texture and material texture
all had instance ID -51232. Five horizon patches (320 pixels) ranged from
RGB (6,51,14) to (148,200,255), with mean (82.50,125.69,135.82).

This confirms actual video visibility in the raw branch and narrows the
failure to the difference from the normal rendering path. It does NOT identify
a specific broken uniform, arithmetic operation or driver/compiler defect.
Do not claim `exp2`, Unity version, decoder or multiview was the root cause.
No normal-mode fix has yet been tested on Quest.

## Narrow orientation change installed

Only the `Nexora.LoadVideo` event in the already-remade device map
`Dynasty-Nexora/ExpertPlusStandardNexora.dat` gained `yaw: 180.0`.
Full Vivify, gameplay notes, custom sabers, the MP4 and bundles are unchanged.
The new DAT is also available in `release/2026-09-09-Dynasty-Video-Orientation/`.

- Original DAT SHA-256:
  `766ed70544f1943973993b54d4ef1909c9eb38e20f79193b00e4038504452c7c`.
- Installed DAT SHA-256:
  `dd351050f1416f74e500ed2b9e0887f1c358556540e1237ac69749f4b44493a9`.
- Local original:
  `diagnostics/nexora-0.3.4-white-2026-09-09/ExpertPlusStandardNexora.original.dat`.
- Headset original:
  `/sdcard/ModData/com.beatgames.beatsaber/Mods/Nexora/Backups/Dynasty-Nexora-20260909/ExpertPlusStandardNexora.dat`.

The updated DAT hash was checked after the atomic file replacement. Its visual
orientation still needs confirmation after restarting Beat Saber; a loaded
SongCore beatmap may be cached. The running game was not stopped.

## Deliberately retained temporary state

The device configuration retains `rawVideoDiagnostic=true` so the next launch
uses the branch the user confirmed visible. `debugLogging` was restored to
false for the next launch; other configuration values were preserved. The
original configuration is in the same diagnostics folder as `nexora.before.json`.

Raw mode bypasses Nexora fragment effects, brightness, exposure and RGB tint.
It does not disable independent Vivify gameplay prefabs/effects, and is NOT a
complete feature-preserving production fix. Do not silently revert this device
to the known-white mode before a replacement rendering path is tested.
