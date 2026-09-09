# Nexora 0.3.5 — Direct Video RC

For **Beat Saber Quest standalone 1.40.8_7379**, Scotland2 / ARM64.

## What this release changes

- Enables `directVideoRendering=true` by default, including upgrades with an
  existing `rawVideoDiagnostic=false`. No configuration file is overwritten
  by the QMOD; the new setting is added at startup while other values remain.
- Selects the same direct shader branch that the user confirmed displays the
  actual Dynasty video on Quest 3S with Nexora 0.3.4. The Android shader bundle
  is unchanged from that test. No speculative decoder or shader rewrite.
- Includes the matching native source, Unity shader source and built Android
  asset. Experimental opt-in RGBD from 0.3.4 remains disabled unless explicitly
  authored; it has not been verified on Quest and needs actual depth media.

## Important limitation

**This is a visible-video workaround, not a full fix of the effects path.**
Nexora fragment RGB tint/brightness/exposure, hue/saturation, ripple, fog,
scanlines, vignette, pixelation, chromatic split, kaleidoscope and shader camera
effects are temporarily bypassed. Video synchronization, pause/seek handling,
opacity/fades, projection/eye packing, orientation and vertex deformation remain
in the active path. Independent Vivify effects, notes and sabers are not disabled.

The exact cause of white output in the normal path is still unresolved. The
user-confirmed hardware result is the direct branch in 0.3.4; a successful
0.3.5 build/package is not a full-device gameplay/stability certification.

## Install / orientation

Install `Nexora-Quest-0.3.5.qmod` and restart Beat Saber. Direct mode is the
default without a manual diagnostic toggle. Explicit opt-out requires both
`directVideoRendering=false` and `rawVideoDiagnostic=false` followed by restart.

The separately installed local Dynasty correction adds `yaw: 180` to that
map's `Nexora.LoadVideo` event because its video faced backwards. **The QMOD
does not rotate every map** and does not contain the Dynasty DAT, MP4 or map
bundle. Those remain outside GitHub. Reload the game to clear cached map data.

Only Nexora is released here. Vivify, Noodle Extensions, AudioLink and Synapse
are not updated or claimed fixed by this release. 42-flux fixes and Synapse's
full port are not included.

Assets: ARM64 QMOD, tagged source snapshot and SHA-256 checksums; no map/media
files, headset logs or macOS metadata.
