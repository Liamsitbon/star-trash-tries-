# Vivify Quest stereo + Murder Plot compatibility patch

This source copy keeps Vivify at the existing 0.6.6 manifest version and adds two narrowly scoped Quest safety fixes.

## Vivify fixes in this copy

1. `_StereoActiveEye` is now treated as a physical-eye selector only when XR is in true MultiPass mode (`stereoRenderingMode == 0`). Quest Multiview/texture-array modes keep the global at `0`.
2. Both image-effect and mid-render command-buffer chains explicitly reset `_StereoActiveEye` to `0` before legacy/regular Blits. The existing opt-in `_VivifyPerEyeBlit` path still selects eye 0/1 locally and resets afterward.

These changes target whole-frame stereo divergence/ghosting where sabers and the scene can appear as two slightly displaced transparent copies.

## Murder Plot is a Noodle ownership bug

Murder Plot (BSR 45d7f) contains thousands of fake color notes. The dense row effects are authored as many copies whose Noodle `dissolve` visibility is normally zero except for the intended visible copy. A hidden copy has visibility `0`, which means cutout `1`.

The current Quest Noodle note path can select the opaque note material using `visibility > 0`. At exactly visibility `0`, it then applies cutout `1` while the opaque material is selected. That makes supposedly hidden fake copies visible as rows. The companion script in `compat/noodle/apply_noodle_murder_plot_fixes.py` changes the material-selection condition to keep the cutout-capable material whenever body or arrow visibility is below `1`.

The companion script also fixes a separate operator-precedence bug in Noodle's beatmap callback condition.

## Intentionally not changed

- Point definitions are **not sorted**. PC Heck preserves authored point order and uses the same binary-search semantics, so sorting would reduce PC parity rather than improve it.
- `OffsetBladeMovementData` is **not changed**. The official PC Vivify implementation intentionally anchors both custom trail endpoints from the original bottom position.
- Legacy Blit materials are **not automatically forced** into the explicit per-eye array path. That path requires shader opt-in (`_VivifyPerEyeBlit`) because a legacy shader may not use the correct texture-array sampler.

## Verification order

Run static/build checks first. Do not intentionally reproduce a known uncomfortable stereo-divergence scene on-headset merely to test the patch. After the headset/eye discomfort is fully gone, normal gameplay testing can be done cautiously.
