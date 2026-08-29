# Friend right-eye video and Aether source evidence

Date analyzed: 2026-08-09 (Asia/Jerusalem)

## Evidence boundary

The two supplied MP4 files are right-eye captures. Their filenames place them
at 01:52:39 and 01:55:35, while Vivify 0.5.30 and Noodle Extensions 1.8.3 were
built and installed after 04:16. They therefore document the pre-fix failure
and cannot validate or invalidate the currently installed binaries.

| Evidence | Duration / size | SHA-256 |
| --- | ---: | --- |
| `com.beatgames.beatsaber-20260809-015239-0.mp4` (42-flux) | 171.708 s / 61,387,655 bytes | `c922d359937483912e016e69c3419f4e1cd3ae7a725a9230192c7d9a8bd66558` |
| `com.beatgames.beatsaber-20260809-015535-0.mp4` (Aether) | 89.391 s / 33,317,999 bytes | `93772a7d448d317ff81eb5c558166cfa6dc79849639df6d381d2362bb3388aa5` |
| `Aether.zip` | 58 MB | `0bfd5caeb2cc888cfe3470d0b9b4a2c25a33fac5d1b440abf5f9857d7d439cae` |

Both recordings are 1280x720 H.264 captures at approximately 60 encoded
frames per second. `blackdetect`, `freezedetect`, selected-frame inspection,
map timing, and the authored custom-event graph were used together; a dark
authored scene was not classified as a runtime failure from brightness alone.

## 42-flux: confirmed pre-fix secondary-camera failure

Audio begins at approximately video time 11.234 s. At 225 BPM, the map beat is
approximately `(videoTime - 11.234) * 3.75`.

The recording becomes effectively black from 26.458 s through 38.141 s, with
only isolated UI/effect fragments visible. This maps to approximately beats
57.1 through 100.9. Frame-freeze detection also finds repeated static runs
through that same interval, including individual runs up to 1.43 seconds.

This is not the authored six-beat blackout:

- The authored `BlackOut` starts at beat 42 and ends at beat 48 (about 1.6 s).
- `NotesOneCam` is created at beat 47 with `_NotesOne` and
  `_NotesOne_Depth`.
- At beat 48, the main camera blacklists `NotesOne`, while the compositor is
  expected to draw the isolated notes back from that secondary camera.
- The observed black interval persists for roughly 53 beats beyond the end of
  the authored blackout and ends near the compositor section's beat-112
  boundary.

The eclipse begins at beat 144, approximately video time 49.634 s, exactly
where the recording first shows the sun/moon section. The map intentionally
moves the sun and eclipse together for 32.5 beats. It does not intentionally
place playable notes behind the opaque moon: those notes are meant to be
isolated and composited with their matching secondary depth. Missing or
zero-valued right-eye color/depth input explains both the long black interval
and notes disappearing behind the eclipse.

This matches the defect found in the 0.5.28 diagnostic log: Unity supplied a
two-slice Multiview `Tex2DArray`, but the old runtime stored the capture as a
single-eye slot and could bind an incompatible mono depth texture. Vivify
0.5.29+ stores a Multiview array in the shared stereo slot, preserves both
color slices, normalizes secondary depth to the same two-slice layout, and
uses owned GPU copies so the main camera cannot overwrite the helper camera's
depth before composition.

The MP4 does not provide a reliable GPU-stall counter. Its repeated frames in
the long black section are consistent with a static/empty render, not by
themselves proof of a GPU driver stall. The supplied 0.5.28 log's 588
`culling CPU stall` warnings are CPU layer/renderer rescans. The unchanged-data
culling cache in 0.5.29+ addresses that measured source of frame pacing loss.

## Aether: strong authored-visual correlation

Gameplay audio begins at approximately video time 6.632 s. At 145 BPM, the map
beat is approximately `(videoTime - 6.632) * 2.4167`.

Representative right-eye frames line up with the original TypeScript source:

| Video time | Approx. beat | Source correlation |
| ---: | ---: | --- |
| 13 s | 15.4 | End of the beat-5, ten-beat IntroBokeh pass |
| 23 s | 39.6 | Bright ice/terrain intro |
| 33 s | 63.7 | Authored transition around beat 64 |
| 43 s | 87.9 | Black/outline-note setup around beat 89 |
| 53 s | 112.1 | White/outline-note transition around beat 112.25 |
| 63-88 s | 136-197 | Authored drop geometry, flares, negative/outline styling |

The only detected Aether black runs longer than 0.2 s are during the silent
pre-song/startup period, plus one 0.25 s visual cut around video time 47 s.
There is no long black or frozen gameplay interval comparable to 42-flux.
The very bright ice, black/white inversions, rainbow rings, outline notes, and
white flashes visible in this right-eye capture are consistent with the
authored source. A persistent single-eye mismatch still requires a simultaneous
left/right headset comparison; this right-eye-only file cannot prove eye
parity.

The original Aether blit shaders are stereo-aware. `Bokeh.shader` and
`SimpleColor.shader` use Unity's stereo vertex output, initialize the stereo
eye, call `UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX`, and sample
`UNITY_DECLARE_SCREENSPACE_TEXTURE` inputs. The Android bundle also contains
Multiview variants. This supports preserving true Quest Multiview and the
native two-slice render graph instead of forcing the map into mono or
MultiPass.

## What `Aether.zip` contains

The ZIP passes an integrity test. It is not a diagnostic package. It contains:

- a ready-to-install Aether map;
- Android, Windows 2019, and Windows 2021 Vivify bundles;
- the complete `aether_map-master` TypeScript map source;
- a Unity 2019.4.28f1 project containing the original prefabs, materials,
  textures, and shader source.

The Android member is byte-for-byte the same bundle already analyzed:

`b407a6e4678e283eb6ca3ef24f0c65f507ec18991c0e183ed0816a939bc8e092`

Consequently it does not introduce a different Android map build. Its value is
the original shader/scene source, which confirms the authored visual timing
and stereo macros. The source project's README retains the creator's asset-use
restrictions; it should not be repackaged into this mod release.

## Current-build verification status

Vivify 0.5.30 and Noodle Extensions 1.8.3 remain installed and hash-verified on
the connected Quest 3S. Meta Horizon currently blocks unattended Beat Saber
launches with `ControllerRequiredDialogActivity`, before Unity and the mod
loader start. The new binaries therefore still require an awake-controller,
two-eye gameplay run before broad distribution.
