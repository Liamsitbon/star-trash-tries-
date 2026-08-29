# Vivify Quest 0.6.2 — Aether timing and XR resume investigation

## Evidence boundary

This record separates the failure reproduced with the installed 0.6.1 binary
from the 0.6.2 source/build candidate. The 0.6.1 failure is confirmed on the
connected Quest 3S. The 0.6.2 source, host tests, ARM64 build, package, and
staged device files are verified. A complete 0.6.2 gameplay pass is still
required before either fix is called headset-confirmed.

Captured evidence:

- `diagnostics/2026-08-11-live-0.6.1-debug-logcat.txt`
- `diagnostics/2026-08-11-180105-0.6.1-aether-sleep-resume-freeze.zip`
- `diagnostics/2026-08-11-180105-0.6.1-aether-sleep-resume-freeze/Vivify.log`
- user recording `com.beatgames.beatsaber-20260811-143514-0.mp4`

## Aether prefab timing failure

The map fires these relevant prefabs:

| Beat | Song time at 145 BPM | Prefab |
| ---: | ---: | --- |
| 77 | 31.862 s | `assets/aether/prefabs/sections/drop.prefab` |
| 261 | 108.000 s | `assets/aether/prefabs/sections/ambient.prefab` |

The 0.6.1 Quest path stored the absolute event time and also passed that value
directly to `Animator.Update`. A prefab fired normally at 31.862 seconds was
therefore evaluated as if 31.862 seconds had already elapsed inside that new
prefab. This explains both reported symptoms:

- the drop opening expected in the recording near 0:41–0:42 was skipped;
- the later ambient prefab was evaluated far beyond its approximately
  46.7-second clip and appeared black/gray near the reported 1:55 section.

The Windows component lifecycle starts a normally fired prefab at local time
zero. Practice or catch-up mode needs the opposite behavior: it must begin at
the elapsed time inside the event, not restart the whole background.

0.6.2 therefore computes:

```text
initialElapsed = max(0, callbackSongTime - eventStartSongTime)
```

The absolute event time is still retained for video synchronization. The
calculation is covered by host tests for normal playback, early callback
delivery, practice catch-up, and invalid time values.

Vivify now prefers the active map's `BeatmapCallbacksController.songTime` for
background timing. This is the same callback clock that fires the chart events
driving notes/cubes, and avoids selecting a stale global
`AudioTimeSyncController` during practice or scene transitions. The audio
controller remains the source of playback speed.

## Sleep/resume freeze reproduced in 0.6.1

The device timeline, not an estimate of wall-clock duration, was:

| Time | Evidence |
| --- | --- |
| 17:48:22.092 | Vivify pause menu opened; 3 synced objects, 0 secondary cameras |
| 17:49:49.976 | Android Activity paused for the long sleep interval |
| 17:58:34.044–34.060 | OpenXR session and stereo swapchains recreated |
| 17:58:40.276 | Vivify pause menu closed; 3 synced objects, 0 secondary cameras |
| 17:58:41–43 | VrApi records repeat byte-for-byte; no new app frames were submitted |
| 17:59:18.266 | Activity was closed after the frozen state |
| 17:59:24.332 | Android sent SIGKILL after Activity close |

The repeated VrApi counters after 17:58:40 are consistent with the compositor
reprojecting one stale frame while audio continues on another thread. There is
no lmkd/OOM record. GPU allocation was high (approximately 891 MiB reported as
100% allocated after resume), but that is context and not proof of the cause.
The later SIGKILL was an exit consequence, not evidence that Android killed
the game for memory pressure.

Aether has one rendering-setting event, at beat zero:

```json
{"qualitySettings":{"antiAliasing":0}}
```

In 0.6.1, opening the pause menu restored the original anti-aliasing value and
closing it synchronously reapplied zero. The reapply occurred immediately
after OpenXR recreated the Quest eye swapchains. Changing Unity anti-aliasing
at that boundary can recreate eye render targets and is the strongest
source-level candidate for the observed main/render-loop stall. The capture
does not include a native thread dump at that exact instruction, so this is a
high-confidence candidate, not a claim of instruction-level proof.

0.6.2 leaves authored render/quality settings intact while the pause menu is
open and restores them only during the existing runtime teardown/restart path.
This also matches the Windows lifetime, where these values remain active until
component disposal. Global shader properties retain their existing scoped
pause behavior.

The resume path also:

- resets the cached callback-clock sample;
- forces Animator/ParticleSystem practice speed to be applied again;
- emits debug-only checkpoints after camera restoration, global-state restore,
  animation restoration, lifecycle resume, camera refresh, and completion.

If a future run freezes again, the last checkpoint will identify the remaining
blocking stage without adding per-frame release overhead.

## 0.6.2 acceptance criteria

The candidate is confirmed only after all of these are observed on the staged
0.6.2 binary:

1. From normal Play, the Aether drop does not skip its opening and the expected
   scene appears at the reported 0:41–0:42 recording position.
2. Starting through Custom Time reconstructs the correct local prefab position
   and the late ambient section is not incorrectly black/gray.
3. After at least one long Quest sleep/resume cycle, music, notes, background,
   and submitted XR frames all continue.
4. Debug logs contain `Vivify resume complete` after every pause-menu close.
5. The installed private and staged library hashes match the 0.6.2 build hash
   recorded in `BUILD_AND_TEST_0.6.2.md`.

