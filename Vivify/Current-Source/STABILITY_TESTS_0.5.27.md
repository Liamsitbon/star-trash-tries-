# Vivify Quest 0.5.27 device regression checklist

Use Beat Saber **1.40.8_7379** on the release Quest target. Start with all four
Vivify feature switches enabled. Save `Vivify.log` after every failed case.

## Startup and stereo invariants

1. Confirm the log reports `stereoMode=3` on the normal Quest Multiview setup
   and `multipassSetting=false`.
2. For each created stereo camera, confirm `colorDimension=5`,
   `colorSlices=2`, and—when present—`depthDimension=5`, `depthSlices=2`.
3. Confirm the settings page contains no Multiview Safety switch.
4. Look through both eyes while rotating and translating the headset; no layer
   may remain fixed to the display unless the map intentionally authors it.

## Target maps and recordings

1. **42 Flux** (`676767` / log reference `777888`): play through the sun,
   eclipse, NotesOneCam, distortion, later Beat Saber image, pause/resume, and a
   full Restart. Notes must remain readable, both eyes must agree, and restart
   must return to gameplay without a crash.
2. **Murder Plot**: play the repeated-effect section. A note must not become a
   row of horizontally repeated cubes and the main view must not recurse.
3. **Aether** (`436152` / log reference `456456`): compare both eyes while
   moving the head. Objects must not duplicate or disappear in only one eye;
   custom sabers must replace stock sabers. Record any static shader fallback.
4. **Algae** (PC reference `060606`, Quest reference `090909`, log `097097`):
   confirm the Android environment bundle loads. Missing custom-note template
   assets may use readable stock notes, but the log must contain only one
   warning per unique missing asset rather than one per note.
5. **Arrow visibility** (`123123123123`): verify every arrow remains readable
   against the black and overexposed white sections in both eyes.

## Lifecycle and soak

1. From 42 Flux, perform pause/resume ten times, Restart five times, return to
   menu, select a non-Vivify song, and then select another Vivify song.
2. Repeat a backward practice seek at a nonzero timestamp; verify Vivify state
   reconstructs without a menu-frame NRE loop.
3. Complete at least three Vivify maps consecutively and run a 30-minute mixed
   soak. Check for crashes, increasing repeated warnings, stuck culling layers,
   frozen eye textures, or stock sabers returning after pause.
4. If a crash occurs, keep both `Vivify.log` and the Android tombstone. Source
   compilation or successful QMOD installation alone is not a pass for this
   checklist.
