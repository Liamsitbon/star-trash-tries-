# Vivify Quest 0.6.1 Meta Quest 3 optimization audit

Date: 2026-08-11

## Scope and evidence boundary

This release optimizes the complete Windows-master port without removing
Vivify event types, render orders, secondary/depth cameras, prefab assignment,
video synchronization or lifecycle safeguards. It does not add a reduced
"Quest mode" and does not silently disable authored effects.

Historical local diagnostics identify the relevant workload as Android 14,
Adreno 740, XR stereo mode 3 (Single Pass Multiview), two-slice texture arrays
and a 2016 x 2112 eye render descriptor. Those captures came from a Quest 3S
running an older build. They justify targeting the same XR/GPU pressure points,
but they are not runtime proof for 0.6.1 and are not a Meta Quest 3 benchmark.

No headset is currently visible through ADB. Every result below is therefore
classified explicitly as source proof, host-test proof or Android build proof.
Frame time, FPS, thermal improvement and crash freedom remain unconfirmed until
the device matrix in `STABILITY_TESTS_0.6.1.md` is run.

## Optimizations

| Area | Repeated work removed | 0.6.1 behavior | Safety boundary |
| --- | --- | --- | --- |
| Mid-render command buffers | Buffer removal, disposal, allocation and full graph reconstruction after every render | Buffers remain attached while generation, owner, camera, descriptor, textures and authored effect graph match | Cache commits only after successful installation; disable, destroy, pause, graph change and every reset invalidate/remove it |
| Mid-render self-blits | Temporary render texture acquisition for a graph that is reused frame after frame | A persistent texture is created with the command graph and destroyed with it | Descriptor/graph change detaches buffers before destroying their targets |
| Immediate image blits | A new `CommandBuffer` for each pre/post effect invocation | One buffer is retained, cleared and reused; temporary self-blit targets are still released after synchronous execution | Late scene teardown clears the managed handle without dereferencing retired native objects |
| Passthrough frames | Command-buffer setup for a plain source-to-destination copy | Direct `Graphics.Blit` | Rendering result remains the same |
| Culling roots | All renderer hierarchies periodically rescanned in the same frame | Deterministic 45–75 frame refresh offsets spread scans; disabled/inactive renderers are not moved | Child-count changes trigger immediate refresh and every root still receives a periodic deep refresh |
| Camera refresh | Reapplying unchanged authored properties and repeated component lookups every update | Main-camera properties apply only after camera/property change; cached controllers are reused while alive | Camera replacement and property events mark/reacquire state; menu-camera and lifecycle guards remain |
| Animator/particle synchronization | Native speed writes driven by noisy song-time/frame-time ratios | Stable `AudioTimeSyncController.timeScale` is used; a non-advancing timeline still yields zero | A newly registered object invalidates the cached rate so it receives the current value immediately |
| Video synchronization | Rewriting identical playback speed every update | Per-object last rate is tracked with a tested epsilon | Pause/play and drift correction paths are unchanged |
| Replacement fingerprints | Concatenated temporary strings and float formatting | Allocation-free 64-bit fingerprints use exact float bits and stable field ordering | Fingerprints are compared only after a previous value was explicitly applied |
| Replacement colors | New managed `MaterialPropertyBlock` for each color application | One mod-lifetime block is reused; Unity copies its value into each renderer | Only the Vivify color property is written |
| Saber integrity | Two temporary `unordered_set` allocations every 30-frame hard-hide check | Small renderer lists use allocation-free linear membership tests | The hard-hide/reassert scan cadence remains unchanged |
| Saber trails | A new property block whenever the followed color changed | Each followed trail retains its block and skips equal colors | Cleanup clears the retained handle |
| File logging | Synchronous flash flush after every informational line | Warnings/errors flush immediately; ordinary lines flush in batches of 32 | Crash-relevant severity remains immediate and the stream flushes on normal shutdown |

The build deliberately remains generic ARM64 with the existing optimized/LTO
configuration. It does not use `-ffast-math`, a Quest-3-only `-mcpu`, forced
dynamic resolution, fixed foveated rendering overrides, refresh-rate overrides
or Android CPU/GPU level overrides. Those changes would either alter authored
semantics or reduce compatibility and require device-specific profiling first.

## Instrumentation

When `vivifyDebugLogging` is enabled, the runtime emits a 900-frame window:

```text
Vivify Quest perf window: frame=... midCacheHits=... midRebuilds=... imageBlits=... imageCommandBufferCreates=...
```

Interpretation:

- on a stable persistent mid-render graph, `midCacheHits` should greatly exceed
  `midRebuilds`;
- `midRebuilds` should rise when an authored effect begins/expires, a target
  changes, or a camera/session changes;
- `imageCommandBufferCreates` should not track `imageBlits`; normally one buffer
  serves a live session and a later lifecycle reset may create another;
- a low rebuild count does not prove visual correctness, so it must be checked
  alongside both-eye recording and the event matrix.

`scripts/collect-quest3-performance.ps1` records model/build data, requested
Oculus tuning properties, process CPU/RSS/VSZ, KGSL busy counters and clock,
thermal-zone maximum, display state, memory snapshots, filtered logcat,
Vivify.log and installed library hashes. Raw samples are retained beside the
normalized CSV so an unexpected device format can be audited.

`scripts/analyze-quest3-performance.ps1` summarizes sample count, minimum,
mean, median, p95 and maximum. With a baseline path it also computes candidate
median deltas. Android `gfxinfo` is retained only as auxiliary evidence because
VR compositor timing is not guaranteed to be represented like a conventional
Android surface.

## Local checks required for release

```sh
scripts/test-port-foundation.sh
scripts/verify-port-source.sh
pwsh -NoProfile -File scripts/build.ps1 -clean
pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.1
```

The host suite covers lifecycle generations, cache invalidation, deterministic
culling spread, rate-change/stable-sync policy under ASan/UBSan, and the
performance CSV analyzer with a synthetic A/B capture. Exact clean-build,
binary and package evidence is recorded in `BUILD_AND_TEST_0.6.1.md`.

## What is not yet proven

- no 0.6.1 frame-time or FPS measurement exists;
- no 0.6.1 thermal or sustained-clock measurement exists;
- no 0.6.1 visual capture from both eyes exists;
- no 0.6.1 gameplay-to-menu, restart, seek or multi-map cycle has run on a
  physical headset;
- source/build success cannot close historical native `Camera::RenderStereo`
  crashes or managed update exceptions.

Those items remain open until the exact packaged library is installed and the
resulting capture includes a matching SHA-256.
