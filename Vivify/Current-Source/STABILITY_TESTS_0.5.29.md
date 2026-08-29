# Vivify Quest 0.5.29 device regression checklist

Target Beat Saber **1.40.8_7379** on the intended Quest. Enable Blit effects,
Beat-0 film grain, Secondary/depth cameras, Prefab prewarming, and Vivify debug
logging for this test build.

## Flux visual/depth checks

1. Start 42 Flux from a fresh Beat Saber launch. Reject the build if the
   opening or any camera-composited section becomes black while only some cubes
   remain visible.
2. At each `CreateCamera`, verify `Vivify secondary capture` reports `eye=2`,
   `vrUsage=2`, `dimension=5`, and `slices=2` on the normal Multiview setup.
3. Verify each `Vivify secondary depth layout` line reports an output with
   `vrUsage=2`, `dim=5`, and `slices=2`. A 2D/one-slice source is allowed only
   when the logged output is the normalized two-slice array.
4. At the sun/eclipse, playable notes and sabers must remain visible as the map
   intends instead of disappearing behind the black moon.
5. Continue past every later NotesCam section. Cubes must not disappear when
   the main camera's authored culling blacklist becomes active.
6. Check both eyes while moving and rotating the headset; no capture may freeze
   to one eye or follow the headset as a flat overlay.

## Performance checks

1. Retain the FPS/performance overlay recording for the full Flux run.
2. Each unchanged main-camera culling configuration should log at most its
   initial diagnostic samples. Reject a return to hundreds of consecutive
   `culling pass` or `culling CPU stall` lines with every root refreshed.
3. Compare the four former freeze windows with the 0.5.28 recording. Record
   minimum FPS, visible freeze duration, and any Android `dequeueBuffer` or
   frame-timeline warnings.

## Lifecycle checks

1. Finish Flux normally. Leave the result screen through Continue and verify
   the menu remains stable for at least 30 seconds.
2. Pause/resume ten times, Restart five times, and return with the menu button
   five times. Start a normal non-Vivify map after every path.
3. Confirm `Vivify detected MainMenu with live beatmap state` is followed by
   `ResetRuntime: transition=true`, with no Vivify Blit stereo runtime line on
   `MenuMainCamera` and no native crash.
4. Run three Vivify maps consecutively, perform a backward practice seek, then
   complete a 30-minute mixed-map soak.

## Other regressions

1. Confirm both custom sabers work on the first launch and after Restart.
2. Test Aether in both eyes and through its previously broken-eye section.
3. Test Murder Plot. If fake notes still form horizontal rows, immediately
   collect the run; do not classify it as fixed by Vivify without Noodle
   dissolve evidence.
4. Immediately after any visual failure, crash, or severe stall run:

   `pwsh -File scripts/collect-vivify-diagnostics.ps1 -label <short-name>`

   Add `-bugreport` after a native crash when Android scoped storage blocks the
   tombstone.
