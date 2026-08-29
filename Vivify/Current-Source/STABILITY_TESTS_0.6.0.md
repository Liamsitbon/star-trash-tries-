# Vivify Quest 0.6.0 headset validation matrix

This checklist is intentionally evidence-oriented. A successful local build or
QMOD install is not a gameplay pass. Record the exact Beat Saber/mod versions,
map, difficulty, headset, stereo mode and attached logs for every run.

## Required environment

- Beat Saber `1.40.8_7379`
- Scotland2
- Vivify `0.6.0`
- dependency versions from the installed QMOD manifest
- no old `libVivify.so` left in another mod directory

Before testing, capture the installed library hash and confirm the in-game log
reports `0.6.0`.

## P0 — historical native-crash regression

1. Launch the exact map/path that previously produced a native crash after
   leaving gameplay (the prior evidence used the Flux run).
2. Finish or exit normally to Main Menu.
3. Remain on Main Menu for at least ten seconds and move through one menu panel.
4. Confirm there is no SIGSEGV in `Material::GetShader`,
   `RenderingCommandBuffer::PPtrResolver<Material>::Put`,
   `AddBlitRenderTarget`, or `CommandBuffer_CUSTOM_Blit`.
5. Confirm Vivify logs one transition reset and no gameplay `ApplyBlits` after
   the active scene becomes Main Menu.
6. Repeat three times, including one fail/retry and one direct quit.

Pass requires all repetitions and retained Vivify/Paper/logcat evidence.

## P0 — beat-zero and both-eye rendering

Use a demanding map with beat-zero Vivify effects (for example the local Murder
Plot regression map if installed).

- Notes and stock-note fallback are visible from the first beat.
- Before/after-main-effect Blits appear in both eyes.
- Every mid-render order used by the map appears in both eyes.
- No one-eye clear, grey/white fallback fullscreen pass, or alternating-eye
  frame occurs.
- Pause/resume does not change eye parity or duplicate an effect.

Record a through-the-lens video or headset capture that can distinguish both
eyes; a desktop mirror alone is insufficient proof.

## P0 — lifecycle sequence

Run this without restarting Beat Saber:

1. Vivify map A, pause, resume, then restart.
2. Seek backward and forward in Practice Mode.
3. Quit to menu.
4. Vivify map B.
5. Quit to menu.
6. A non-Vivify map.

Confirm no stale prefab, skybox/sun, camera texture, culling layer, shader
global, Blit or command buffer survives into the next stage. The vanilla map
must have normal camera effects, sabers, trails and stock notes.

## P1 — secondary camera parity

Exercise at least one map/event for each item:

- color-only secondary camera;
- color plus depth output;
- explicit `mainEffect: false`, unset and `null` (unset/null must be enabled);
- BloomPrePass false then null/true;
- clear flags and background color set, then explicitly reset with null;
- multiple depth flags; authored flags must extend the base depth requirement;
- culling whitelist and blacklist with dense animated tracks;
- Multiview/Single-Pass and, if available, MultiPass.
- duplicate `CreateCamera` and `CreateScreenTexture` ids before destruction;
  the second declaration must be rejected rather than replace the first;
- set a camera property, destroy the camera, then recreate the same id; the
  keyed property state must be applied to the recreated camera.

Inspect the output texture in both eyes and check that a destroyed/recreated
camera does not reuse a prior map's material or culling state.

## P1 — object prefab parity

- Assign one prefab to multiple tracks and confirm every track receives it.
- Reassign while notes are already alive; all matching live notes refresh.
- Test `Single` null and `Additive` null stock-object reveal behavior.
- Test notes, any-direction notes, bombs, debris, sabers and trails.
- Enable left-handed mode and confirm instantiated prefab transforms mirror
  once, without mirroring the whole scene or swapping twice.
- Follow with a vanilla map and confirm base saber-trail enable/disable behavior
  is unchanged.
- Stress a dense replacement-prefab/trail section while recording headset
  frame time and memory; Quest uses lifecycle-safe Unity allocation rather than
  the desktop replacement-object pool.

## P1 — synchronization and settings

- Instantiate a play-on-awake video after song start; it must begin at time
  since instantiation, not absolute song time.
- Pause/resume, change practice speed and seek; video, animator and particles
  remain aligned within the expected drift threshold.
- Exercise literal and animated RenderSettings/QualitySettings/XRSettings,
  skybox and directional-sun changes.
- Exit and confirm all captured settings restore.

## Evidence to retain

- `/sdcard/ModData/com.beatgames.beatsaber/Logs/Vivify.log`
- Paper log for the same process
- filtered logcat containing scene changes and any Android fatal signal
- tombstone or bugreport after a native crash
- installed `libVivify.so` SHA-256
- both-eye video/screenshots for visual failures

Use `scripts/collect-vivify-diagnostics.ps1`; add `-bugreport` if Horizon OS
prevents direct tombstone access.
