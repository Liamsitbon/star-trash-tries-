# Aether Quest analysis for Vivify 0.5.28

## Evidence boundary

- The supplied `456456.log` identifies itself as Vivify `0.5.18`; it is useful
  historical evidence but cannot prove the state of the current Quest port.
- The current Android map bundle was inspected directly. Its SHA-256 is
  `b407a6e4678e283eb6ca3ef24f0c65f507ec18991c0e183ed0816a939bc8e092`.

## Map workload

The current ExpertPlusLawless difficulty contains 3,491 Vivify custom events:

- 1,806 `AnimateTrack`
- 1,363 `AssignObjectPrefab`
- 236 `AssignPathAnimation`
- 65 `SetMaterialProperty`
- 6 `InstantiatePrefab`
- 6 `DestroyObject`
- 4 `Blit`
- 1 `SetCameraProperty`

There are no `CreateCamera` or `CreateScreenTexture` events in this map. The
eye-specific Aether fault is therefore separate from Flux's NotesCam lifecycle
and secondary-camera capture path.

The 1,363 prefab assignments occur at beat zero. Version 0.5.28 indexes these
assignments by object type and track so each spawned note/debris object does
not scan the complete assignment list.

## Full-screen effects and true Multiview

The Bokeh events at beats 5 and 261 use
`Swifter/Aether/Blit/Bokeh`; the white fades use
`Swifter/Aether/Blit/SimpleColor`. The Android bundle contains compiled
`STEREO_MULTIVIEW_ON` variants for the Bokeh shader (as well as the expected
stereo keyword set), so true Quest Multiview must remain enabled.

Vivify 0.5.28 does not force `MULTIPASS_ENABLED` while Unity reports stereo
mode 3. It also records a one-time `Vivify Blit stereo runtime` diagnostic from
inside the live render callback. A fresh 0.5.28 Aether log is required to tell
whether Unity selected the Multiview keyword and a two-slice texture at the
actual failing frame; no built-in XR keyword is overridden speculatively.

## Required live confirmation

After installing the final 0.5.28 artifact, capture both the visual result and
the first `Vivify Blit stereo runtime` line for the Bokeh material. Also retain
the `Vivify prefab lookup indexed` line and the complete diagnostic ZIP. This
is the minimum evidence needed before changing Unity-managed stereo keywords
or claiming Aether fixed on hardware.
