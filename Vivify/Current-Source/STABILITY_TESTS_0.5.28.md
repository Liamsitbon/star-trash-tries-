# Vivify Quest 0.5.28 device regression checklist

Use Beat Saber **1.40.8_7379** on the release Quest target. Start with Blit
effects, Beat-0 film grain, Secondary/depth cameras, and Prefab prewarming
enabled. Keep Vivify debug logging enabled during this release test.

## Startup and stereo invariants

1. Confirm the log reports `stereoMode=3` on the normal Quest Multiview setup.
2. For every `CreateCamera`, confirm the first capture reports `eye=2`,
   `dimension=5`, and `slices=2`. It must say `targetlessStereo=true`.
3. Confirm the settings page contains no Multiview Safety switch.
4. Look through both eyes while rotating and translating the headset. No map
   layer may stay attached to one display unless the map authors that behavior.
5. In Aether, retain the first `Vivify Blit stereo runtime` line for the Bokeh
   material and the `Vivify prefab lookup indexed` line. These identify the
   live stereo keyword state and the size of the indexed assignment workload.

## Target maps and recordings

1. **42 Flux** (`676767`, `777888`, and the 2026-08-09 WhatsApp video): play
   past the sun/eclipse at about 42 seconds and the NotesCam section near 50
   seconds. Notes and sabers must render above the eclipse, the environment
   must not turn into a black or repeated-note layer, and both eyes must agree.
2. In Flux, pause/resume, Restart, then return via the menu button. Verify the
   game does not crash and the next song opens normally.
3. **Aether** (`436152`, `456456`): compare both eyes while moving the head.
   Effects and notes must not duplicate/disappear in only one eye. Both custom
   sabers must appear on the first play without a Beat Saber restart.
4. **Murder Plot**: verify a note never becomes a horizontal row of repeated
   cubes and the main camera never recursively samples itself.
5. **Algae** (`060606`, `090909`, `097097`): verify the Android environment
   bundle. An actually missing prefab asset may retain a readable stock object,
   but the log must warn once per unique missing asset.
6. **Arrow visibility** (`123123123123`): every arrow must remain readable in
   both the black and overexposed white sections.

## Frame pacing and lifecycle

1. In Flux, record the FPS overlay through all four previously observed freeze
   windows. Compare low FPS and freeze duration with the 0.5.27 recording.
2. Check each culling diagnostic: `cachedRenderers` may be large, while
   `refreshedRoots` should normally become zero and `activeMovedGOs` should be
   limited to visible gameplay objects.
3. Pause/resume ten times, Restart five times, return to menu five times, then
   alternate a non-Vivify and Vivify map.
4. Perform a backward practice seek from a nonzero timestamp.
5. Run three Vivify maps consecutively and a 30-minute mixed soak. Reject the
   build for crashes, recurring allocation/depth warnings, stuck culling
   layers, frozen eye textures, or sabers reverting to stock.
6. Immediately after any failure run
   `pwsh -File scripts/collect-vivify-diagnostics.ps1 -label <short-name>`.
