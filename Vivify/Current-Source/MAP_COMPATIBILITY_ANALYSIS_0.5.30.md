# Aether, 42-flux, and Murder Plot compatibility analysis

This report records the map, AssetBundle, video, log, and crash evidence used
for Vivify 0.5.30 and Noodle Extensions 1.8.3. It intentionally distinguishes
authored effects from runtime failures.

## Bundle validation

| Map | Android bundle SHA-256 | Size | Referenced assets |
| --- | --- | ---: | --- |
| Aether | `b407a6e4678e283eb6ca3ef24f0c65f507ec18991c0e183ed0816a939bc8e092` | 2,958,984 | 33/33 found |
| 42-flux | `61c4d2ba70e7851c920abdbbd8d2e18e2d300b1983a56a72e4e1ac6b1d4b0fe8` | 6,846,652 | 55/55 found |
| Murder Plot | `568bf56d34eea28c4aa6cbb904586c6322e01470ef8b5b727562b24bf4773a15` | 1,569,779 | 20/20 found |

All three bundles contain Android GLES/Vulkan data and multiview-capable shader
variants. No missing map-to-bundle asset reference explains the reported black
screens, missing notes, eclipse occlusion, or opaque fake-note rows. No Android
geometry-shader stage is used by these three bundles, so the intentionally
fail-closed `VivifyGeometry` compatibility stub is not the active cause.

## Aether

The two supplied difficulties contain the same Vivify scene/effect structure:

- Standard Expert: 444 real notes, 16 fake notes, and 2,214 custom events.
- Lawless Expert+: 703 real notes, 16 fake notes, and 3,491 custom events.
- Six scene prefabs are instantiated across the song and explicitly destroyed
  at their authored section boundaries.
- Blits include IntroBokeh at beat 5, FadeWhite at beats 253 and 501, and
  another Bokeh pass at beat 261.

Brief bokeh/white transitions at those beats are authored. A persistent white
field, a single broken eye, or a scene that remains absent after the effect is
not authored. The supplied Android bundle has every referenced Aether asset and
all 35 inspected shaders contain multiview variants. The runtime path therefore
uses Vivify's stereo-array preservation and per-eye capture fixes introduced in
0.5.28/0.5.29; a new two-eye headset recording is required to prove parity.

The later right-eye-only `015535` recording begins gameplay audio around video
time 6.632 s. Its ice scene, beat-64 transition, black/outline notes near beat
89, and white/outline transition near beat 112 line up with the original
TypeScript source. It contains no persistent in-song black interval. The
supplied `Aether.zip` is the full Unity/TypeScript source project rather than a
diagnostic package, and its Android bundle is byte-for-byte the same
`b407a6...` bundle listed above. See `FRIEND_RIGHT_EYE_EVIDENCE_0.5.30.md` for
the timing and source inspection.

## 42-flux

Expert+ contains 877 real notes, 1,286 fake notes, and 1,602 custom events. Its
visual graph deliberately isolates notes into secondary cameras and composites
them over the main scene:

- Beat 42 starts a six-beat `BlackOut` effect. At 225 BPM this is about 1.6
  seconds and is authored; a black view persisting beyond it is not.
- Beat 47 creates `NotesOneCam` with color/depth textures and a track whitelist;
  beat 48 blacklists the same notes from the main camera while compositing the
  secondary result.
- Beat 144 creates the eclipse and sun and starts a multistage bloom chain.
- Beat 174 creates `NotesCam`; beat 175 blacklists the tiling notes/sabers from
  the main camera; beat 176.5 destroys the sun/eclipses/camera and clears the
  culling override. Similar camera sections begin around beats 205 and 300.

Consequently, notes being hidden behind the moon or disappearing during later
tiled sections is not authored. It means the secondary color/depth texture or
the compositor did not survive Quest multiview. Vivify 0.5.29 preserves both
texture-array slices, duplicates mono depth into a two-slice destination when
necessary, and avoids rebuilding unchanged culling data every frame. The
diagnostic ZIP predates those changes: its loaded binary is 0.5.28.

The later right-eye-only `015239` recording provides an exact visual match for
that diagnosis. Gameplay audio begins around video time 11.234 s. The screen
is effectively black from 26.458 s through 38.141 s, approximately beats 57.1
through 100.9 at 225 BPM. The authored beat-42 blackout should end at beat 48;
the extra interval instead spans the `NotesOneCam` composition section. The
sun/eclipse appears at video time 49.6 s, matching beat 144. These captures
predate the 0.5.29/0.5.30 builds and therefore show the failure those builds
target, not their runtime result.

The 588 measured `culling CPU stall` warnings in that ZIP are CPU renderer/layer
rescans, not proof of a GPU driver stall. The unchanged-data cache in 0.5.29+
prevents that full refresh on every `Update` while retaining authored culling.

## Murder Plot

Lawless Expert+ contains 912 real notes, 3,851 fake notes, 51 fake obstacles,
and 938 custom events. Starting around beat 44, real notes link to time-offset
fake copies used to sequence the animation. Those copies are not meant to form
a visible row: the correct result is one visible cube at a time. Their dissolve
point definitions stay mostly between `0` and about `0.29` and return to `0`,
so the copies depend on the cutout-capable note material throughout their
lifetime.

Noodle Extensions used `dissolve > 0` to decide whether to select the
cutout-capable material. Noodle values are visibility values: `0` means fully
hidden and produces a cutout of `1`. At exactly that boundary the old code
selected the opaque material, so the supposedly hidden copies appeared
simultaneously as full cubes in rows. Noodle Extensions 1.8.3 now keeps the
cutout material selected whenever either body or arrow visibility is below
`1`, matching the existing mirror-note implementation and restoring the
intended one-cube-at-a-time sequence.

## Crash and lifecycle evidence

The supplied diagnostic ZIP loaded Vivify 0.5.28. Its crash is a `SIGSEGV` in
Unity's `RenderingCommandBuffer::AddBlitRenderTarget`, called from
`CameraApplier::OnRenderImage` after `MenuMainCamera` had appeared. This is
consistent with a gameplay command buffer retaining a destroyed authored
material during result/menu teardown, rather than an out-of-memory failure.

Vivify 0.5.29 rejects render callbacks during reset/menu transitions and does
not attach gameplay rendering to `MenuMainCamera`. Version 0.5.30 additionally
removes mid-render command buffers synchronously in `CameraApplier::OnDisable`,
covering completion/retry/exit paths where Unity does not issue the matching
`OnPostRender` callback.

## Verification boundary

Both new libraries were built, packaged, hash-verified, and installed to the
connected Quest's internal and ModData loader locations. Horizon blocked the
unattended app launch with `ControllerRequiredDialogActivity`; therefore this
report does not claim gameplay, two-eye, performance, completion, or restart
validation. Complete `STABILITY_TESTS_0.5.30.md` with the headset and controller
awake before distributing the build broadly.

The two friend recordings were captured before the new builds existed and are
right-eye-only. They cannot replace the required current-build two-eye test.
