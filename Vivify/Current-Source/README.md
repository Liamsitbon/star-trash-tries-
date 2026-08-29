# Vivify Quest 0.6.6 — opt-in per-eye post-processing on Quest

Vivify 0.6.6 builds on the independently audited 0.6.0 source port, the Quest 3
optimization work in 0.6.1, the timing/XR resume fixes in 0.6.2, and the
hardware-confirmed Aether note-stall fix in 0.6.3. It retains the per-eye camera
synchronization from 0.6.4 while adding immediate pause/system-menu freezing,
prewarming for negative-time opening prefabs, and releasing Vivify's own
MetaCore score-submission block from 0.6.5. Version 0.6.6 adds an authored,
opt-in two-slice Blit path for full-screen effects that need a distinct left and
right source layer while Quest remains in efficient Multiview mode. The paired
Noodle Extensions 1.8.5 build fixes
V3 fake-object loading before Vivify prefab and replay processing. The original port is from the
supplied Windows `Vivify-master.zip` to Beat Saber Quest **1.40.8_7379**
(Scotland2). The Windows source identifies itself as Vivify 1.1.0; the ZIP
comment identifies the snapshot as `f83aa3c51290e288edae872dc2dac6b66112d3d6`.

This branch deliberately uses the old Quest project only for Android/QPM,
IL2CPP and stereo-rendering foundations. Runtime ownership, scene teardown and
Windows-master behavior were rebuilt or audited against source. See
[`PORT_PARITY_0.6.0.md`](PORT_PARITY_0.6.0.md) for the foundational feature
matrix, [`QUEST3_OPTIMIZATION_0.6.1.md`](QUEST3_OPTIMIZATION_0.6.1.md) for the
optimization baseline, [`AETHER_RESUME_FIX_0.6.2.md`](AETHER_RESUME_FIX_0.6.2.md)
for timing/resume evidence, and [`AXO_NOTE_STALL_FIX_0.6.3.md`](AXO_NOTE_STALL_FIX_0.6.3.md)
for the note-stall change and provenance boundary. See
[`AXO_FIX_AUDIT_AND_STEREO_FIX_0.6.4.md`](AXO_FIX_AUDIT_AND_STEREO_FIX_0.6.4.md)
for the complete authentic Axo fix comparison and the per-eye change, and
[`QUEST_FAKE_NOTES_PAUSE_UPLOAD_FIX_0.6.5.md`](QUEST_FAKE_NOTES_PAUSE_UPLOAD_FIX_0.6.5.md)
for the 42 Flux/Murder Plot source audit and lifecycle/upload changes, and
[`PER_EYE_BLIT_FIX_0.6.6.md`](PER_EYE_BLIT_FIX_0.6.6.md) for the new shader contract.

## Evidence status

| Layer | Status |
| --- | --- |
| Windows-master source comparison | Completed for all 11 Vivify custom-event types and relevant runtime hooks |
| Host lifecycle and performance-policy tests | Passed with ASan/UBSan where applicable |
| Clean Android ARM64 build | See [`BUILD_AND_TEST_0.6.6.md`](BUILD_AND_TEST_0.6.6.md) |
| QMOD structure/integrity | See [`BUILD_AND_TEST_0.6.6.md`](BUILD_AND_TEST_0.6.6.md) |
| Aether 0.6.1 failure reproduction | Captured on Quest 3S; see the 0.6.2 fix record |
| Axo 0.4.1 commit provenance and source mapping | Verified; see the 0.6.3 fix record |
| Aether bridge with exact 0.6.3 binary | **User-confirmed pass on Quest 3S; matching runtime/hash evidence preserved** |
| Physical per-eye Blit/Murder Plot/42 Flux/pause/upload gameplay of 0.6.6 + Noodle 1.8.5 | **Not yet verified** |

“Implemented in source” does not mean “confirmed fixed on a headset.” In
particular, older `PATCH_NOTES_0.5.x.md`, `BUILD_AND_TEST_0.5.x.md`, and
`STABILITY_TESTS_0.5.x.md` files are retained only as historical material.
Their claims are not accepted as evidence for 0.6.3.

## Quest 3 optimization surface

- immutable mid-render command buffers are retained until their lifecycle
  generation, owner, camera, descriptor, texture graph or authored effect changes
- the immediate image-effect path reuses one command buffer; passthrough frames
  use direct `Graphics.Blit` without creating a buffer
- self-blit render targets tied to a persistent command graph are retained and
  destroyed with that graph instead of being acquired every rendered frame
- culling renderer-cache refreshes are spread deterministically over 45–75
  frames, avoiding a once-per-second all-root scan burst
- unchanged camera properties, practice speed, video playback rate, trail color
  and replacement fingerprints no longer produce redundant native work
- `AssignObjectPrefab` refresh uses the registered Beat Saber note-controller
  pool and only visits notes on affected tracks; it no longer calls
  `FindObjectsOfType` for every authored note-prefab event
- replacement colors reuse a `MaterialPropertyBlock`, saber integrity checks no
  longer allocate temporary hash sets, and normal file-log flushes are batched
- debug mode emits a 900-frame optimization heartbeat, and
  `scripts/collect-quest3-performance.ps1` captures comparable ADB samples

## Ported runtime surface

- Android Vivify asset bundles and deterministic asset lookup
- prefab instantiate/destroy, track attachment and left-handed mirroring
- note, debris, saber and saber-trail prefab assignment
- material, global shader, animator and rendering-setting changes/animation
- all eight authored Blit orders, including duplicate and equal-priority passes
- declared screen textures and stereo secondary color/depth cameras
- separate left/right XR view and projection matrices on secondary cameras,
  with Unity-managed stereo culling instead of a forced centre-eye cull
- camera culling, BloomPrePass and Beat Saber main-effect routing
- video, animator and particle synchronization across pause/seek/restart
- practice reconstruction and generation-owned scene teardown
- `AlwaysVisibleQuad` handling and scoped saber-trail lifecycle ownership

Quest keeps its platform-specific texture-array and per-eye capture path.
Desktop-only Camera2 and desktop mirror-mod integrations are not loaded on
Quest; they are not substitutes for headset XR rendering.

## Build

Requirements: QPM-RS, PowerShell and the configured Android NDK.

```sh
qpm restore
scripts/test-port-foundation.sh
pwsh -NoProfile -File scripts/build.ps1 -clean
scripts/verify-port-source.sh
pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.6
```

The optimized ARM64 library is `build/libVivify.so`; the unstripped diagnostic
binary is `build/debug/libVivify.so`.

## First headset validation

Run [`STABILITY_TESTS_0.6.5.md`](STABILITY_TESTS_0.6.5.md), plus the per-eye
checks in [`PER_EYE_BLIT_FIX_0.6.6.md`](PER_EYE_BLIT_FIX_0.6.6.md). The primary tests
cover Murder Plot fake notes, all 42 Flux scene boundaries, pause/Continue,
Quest Home/sleep, and BeatLeader/ScoreSaber submission status. Preserve
Vivify.log, NoodleExtensions.log, leaderboard logs and any Android tombstone;
local build success cannot replace headset evidence.

For an A/B performance capture on an attached Quest 3, run:

```sh
pwsh -NoProfile -File scripts/collect-quest3-performance.ps1 -durationSeconds 60 -label vivify-0.6.6-flux-pause
```

Runtime log:

`/sdcard/ModData/com.beatgames.beatsaber/Logs/Vivify.log`

After a failure, `scripts/collect-vivify-diagnostics.ps1` can gather the mod,
Paper and Android crash evidence into one ZIP.
