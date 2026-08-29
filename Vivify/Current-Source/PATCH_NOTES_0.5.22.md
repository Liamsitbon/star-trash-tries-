# Vivify Quest 0.5.22 — Multiview Blit safety and practice-start catch-up

## Fixed black fullscreen output with Multiview safety enabled

`UseQuestMultiviewBlitBypass()` now follows the `Multiview safety` toggle on
Quest Single-Pass Multiview (stereo mode 3). With safety enabled, Vivify skips
the unsafe fullscreen CommandBuffer Blit chain and preserves Beat Saber's
normal camera output. Turning safety off restores the full Blit path.

## Fixed large prefab allocation bursts when starting later in a song

Practice/seek starts no longer synchronously prewarm every historical prefab.
Only prefab events at or after the selected song time are eligible for prewarm.

Catch-up now reconstructs only `InstantiatePrefab` events that are still alive
at the selected time. Historical prefab instances that were destroyed before
the selected time are not created and destroyed again in one frame.

Historical post-processing events are evaluated immediately during a seek so
expired effects are discarded instead of filling the staggered beat-0 queue.

## Packaging

- Version: 0.5.22
- `_QPVersion`: 1.2.0
