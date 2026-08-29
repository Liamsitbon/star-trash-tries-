# Nexora custom-event reference

Events use Beat Saber v3 objects: `{ "b": beat, "t": type, "d": data }`.
`id` defaults to `main`; invalid values are clamped and an event failure is
logged without stopping unrelated gameplay systems.

## Dome and playback events

| Event | Important data |
|---|---|
| `Nexora.CreateDome` / `Nexora.SetDome` | visual or transform fields |
| `Nexora.LoadVideo` | map-relative `media`, `autoplay`, `loop`, `syncToSong`, `videoOffset`, `speed`, visual fields |
| `Nexora.PlayVideo` | `time`, `syncToSong`, `videoOffset`, `eventStartSongTime` |
| `Nexora.PauseVideo` / `Nexora.StopVideo` | `id` |
| `Nexora.SeekVideo` | `id`, `time` |
| `Nexora.SetPlayback` | `loop`, `syncToSong`, `videoOffset`, `speed` |
| `Nexora.AnimateDome` | visual target, `durationSeconds`, `ease` |
| `Nexora.Transition` | `opacity`, `durationSeconds`, `ease`; all domes |
| `Nexora.Pulse` | `pulse`, `brightness`, `durationSeconds` |
| `Nexora.Shockwave` | `ripple`, `rippleFrequency`, `durationSeconds` |
| `Nexora.DestroyDome` / `Nexora.DestroyAll` | `id` for one dome; none for all |

Visual fields: `radius`, `opacity`, `brightness`, `exposure`, `saturation`,
`hueShift`, `yaw`, `pitch`, `roll`, `scaleX`, `scaleY`, `scaleZ`, `deform`,
`deformFrequency`, `deformSpeed`, `ripple`, `rippleFrequency`, `rippleSpeed`,
`twist`, `pinch`, `pulse`, `kaleidoscope`, `pixelate`, `chromatic`, `scanline`,
`vignette`, `fog`, `tint`, `offset`, `followPlayer`, `projection`, `flipX`,
`flipY`, `swapEyes`, and `blend`.

Projection aliases are `mono`; `topBottom`, `overUnder`, `tb`, `ou`; and
`sideBySide`, `sbs`. Names are case-insensitive. `blend` is `normal` or
`additive`. `media` is map-relative and normally
`Nexora/Media/world-360.mp4`.

## Camera events

`Nexora.SetCameraEffect` applies immediately.
`Nexora.AnimateCameraEffect` interpolates over `durationSeconds`.
`Nexora.GlitchBurst` applies a short burst and returns to previous values.
`Nexora.ClearCameraEffect` clears the values.

Fields: `amount`, `fisheye`, `chromatic`, `glitch`, `vignette`, `scanline`,
`pixelate`, `grayscale`, `exposure`, `hueShift`, `split`, `shake`, `swirl`,
`kaleidoscope`, `tint`, `durationSeconds`, and `ease`.

On Quest these values are evaluated inside every Nexora dome material. They
affect the cinematic video world without a framebuffer copy and therefore do
not compete with Vivify for `OnRenderImage` or break Multiview. Keep `amount`
at zero when the effect is invisible.

## Cross-fade recipe

Create A at opacity 1 and B at opacity 0, preload B, animate A to 0 and B to 1
with the same duration, then destroy A. This avoids opening a decoder on the
exact transition frame.
