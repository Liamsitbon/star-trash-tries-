# Vivify Quest 0.5.30 device regression checklist

Use Vivify 0.5.30 and Noodle Extensions 1.8.3 together on Beat Saber
1.40.8_7379. Keep Vivify debug logging enabled during this validation run.

## Required map checks

1. **42-flux Expert+**: verify both eyes from map start through the beat-42
   blackout, that notes remain visible in front of the eclipse around beat 144,
   and that the later tiled-note sections do not lose notes.
2. **Aether Expert+ Lawless**: verify both eyes at the intro bokeh, the scene
   changes around beats 77/261/373/509/573, and both FadeWhite sections.
3. **Murder Plot Expert+ Lawless**: verify exactly one cube is visibly presented
   at a time. The time-offset fake-note data must remain cut out; no opaque,
   translucent, horizontal, or depth-stacked row may appear.

## Lifecycle checks

For each map, perform a normal completion, Continue/Retry, pause then menu exit,
and at least one immediate restart. Repeat the sequence twice without closing
Beat Saber. There must be no black frame that persists past an authored effect,
no crash, and no custom-saber loss on the next level.

## Evidence to preserve

Collect `Vivify.log`, `NoodleExtensions.log`, the current Paper/loader log, and
any crash tombstone/minidump. A short through-the-lens video of the exact Flux
eclipse and Murder Plot single-cube sequence is required for visual parity
claims.
