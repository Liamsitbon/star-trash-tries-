# Vivify Quest 0.6.1 headset stability and performance matrix

Status: not run on hardware. This document is an execution contract, not a list
of fixes claimed from a successful compile.

## Required environment record

Record all of the following before accepting a run:

- headset model (Meta Quest 3 or the actual connected model);
- headset OS/build, Android release and SDK;
- Beat Saber `1.40.8_7379`, Scotland2 and every installed mod version;
- display refresh rate and any GraphicsTweaks/Oculus resolution, foveation,
  CPU/GPU-level or refresh-rate override;
- map name/hash, difficulty and exact reproducible checkpoint;
- QMOD SHA-256, installed `libVivify.so` SHA-256 and the version shown by the
  loader;
- whether `vivifyDebugLogging` was enabled.

Do not compare captures with different map sections, modifiers, refresh rates,
room temperature, battery/charging state or graphics overrides. Give the device
the same cool-down and warm-up period before each A/B sample.

## Prepare logging

With Beat Saber closed, set this key in
`/sdcard/ModData/com.beatgames.beatsaber/Configs/Vivify.json`:

```json
"vivifyDebugLogging": true
```

Keep all normal Vivify features enabled. Reopen Beat Saber and verify the log
reports Vivify `0.6.1`. Do not use an old log or a library whose installed hash
does not match the release checksum.

## P0 lifecycle and crash matrix

Each row must preserve Vivify.log, Paperlog, filtered logcat and any tombstone.
A blank result means not tested, not passed.

| ID | Test | Required repetitions | Pass condition | Result |
| --- | --- | ---: | --- | --- |
| L1 | Cold launch, enter a non-Vivify map, finish and return to menu | 2 | No Vivify state activates; no crash/update exception | |
| L2 | Cold launch, enter the chosen high-complexity Vivify map | 2 | Bundle/event preparation completes; both eyes remain coherent | |
| L3 | Finish the Vivify map, return to Main Menu and wait 30 seconds | 5 | No stale camera/material/command-buffer callback or native crash | |
| L4 | Pause and resume during active Blit + secondary camera content | 10 | No stuck culling layer, blank eye, speed drift or duplicated render | |
| L5 | Restart the song from pause during active content | 10 | One fresh generation; old buffers/textures/prefabs never reappear | |
| L6 | Practice seek backward across instantiate/assign/blit events | 5 | State reconstructs once and video/animator/particle sync recovers | |
| L7 | Fail, restart, then exit to menu | 5 | No teardown crash and no hidden stock note/saber in the next song | |
| L8 | Alternate two different Vivify maps, returning through menu | 5 cycles | No asset/state ownership leaks across map generations | |
| L9 | Vivify map followed immediately by a non-Vivify map | 5 | No Vivify camera component/effect/prefab remains active | |
| L10 | Suspend/resume the headset while paused in a Vivify map | 3 | Rendering and sync resume or fail safely without native crash | |

Any new `NullReferenceException`, repeated `Runtime::Update` failure, Android
fatal signal, `Camera::RenderStereo` tombstone, stale `CommandBuffer.Blit`, or
unrestored renderer layer is a P0 failure even if gameplay audio continues.

## P0 rendering/feature matrix

Use maps or a purpose-built fixture that actually exercises every row.

| ID | Surface | Required observation | Result |
| --- | --- | --- | --- |
| R1 | Before/after main-effect Blits | Correct order, duration and expiry in both eyes | |
| R2 | Six mid-render orders | Correct camera-event ordering; stable effects generate cache hits | |
| R3 | Equal-priority and duplicate Blits | Every authored pass executes in source order | |
| R4 | One-frame Blit | Present for exactly the intended frame; graph is removed afterward | |
| R5 | Main-to-main and declared-texture self-blit | No undefined self-copy, corruption or retained destroyed texture | |
| R6 | CreateScreenTexture | Correct texture-array dimensions/format and shader binding | |
| R7 | Secondary color camera | Target is updated without an unnecessary headset-output blit | |
| R8 | Secondary depth camera | Depth is coherent and cleanup restores every temporary layer | |
| R9 | Culling whitelist/blacklist with dense tracked roots | Correct objects in both eyes; no periodic visible CPU hitch | |
| R10 | `mainEffect` true/false and BloomPrePass true/false | Authored flags are honored and restored | |
| R11 | Note/debris/saber/trail assignments | No stock/custom duplication; additive mode remains additive | |
| R12 | Left-handed mode | Mirroring, saber types, trails and colors remain correct | |
| R13 | Video + animator + particle sync | Normal, 80%, 100% and available practice speeds; pause/seek recover | |
| R14 | Global/material/animator/render-setting animation | No behavioral reduction from 0.6.0 | |

Record a headset video that makes left/right-eye divergence, stripes, blank
frames and duplicate geometry visible. A desktop mirror alone is insufficient.

## Comparable Quest 3 performance capture

Create a baseline with the retained 0.6.0 release and a candidate with 0.6.1.
Start each command only after entering the same stable high-load checkpoint:

```sh
pwsh -NoProfile -File scripts/collect-quest3-performance.ps1 \
  -durationSeconds 60 -intervalMilliseconds 500 \
  -label vivify-0.6.0-baseline

pwsh -NoProfile -File scripts/collect-quest3-performance.ps1 \
  -durationSeconds 60 -intervalMilliseconds 500 \
  -label vivify-0.6.1-candidate
```

Then summarize the two unzipped capture directories:

```sh
pwsh -NoProfile -File scripts/analyze-quest3-performance.ps1 \
  -path diagnostics/<candidate-directory> \
  -baseline diagnostics/<baseline-directory>
```

Run at least three paired samples. If 72 Hz, 90 Hz or 120 Hz is being claimed,
record and test that actual setting separately; do not mix them into one result.

Review:

- median and p95 process CPU percentage;
- median and p95 KGSL GPU busy percentage;
- median/p95 RSS and end-minus-start memory behavior;
- GPU clock and maximum thermal-zone reading over sustained load;
- visual correctness and crash logs during the exact same interval;
- `Vivify Quest perf window` cache-hit/rebuild/create counters.

The normalized CSV can be empty for a metric if the headset firmware denies or
formats a sysfs field differently. In that case use `samples-raw.txt` to adapt
the parser and mark the metric unavailable; do not replace it with zero.

## Acceptance rule

0.6.1 is device-validated only when:

1. every P0 lifecycle and feature case passes on the exact packaged hash;
2. no historical crash signature or repeated update exception appears;
3. both-eye output retains the complete 0.6.0/Windows-master behavior;
4. three comparable A/B pairs show no material CPU, GPU, memory or thermal
   regression, with raw captures retained;
5. the command-buffer heartbeat is consistent with graph reuse rather than
   silent effect removal;
6. installed-library and release hashes match.

Until then, report 0.6.1 as source-tested and Android build/package-validated,
not gameplay-validated or performance-proven.
