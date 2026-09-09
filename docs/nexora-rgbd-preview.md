# Nexora 0.3.4: RGBD and video-rendering A/B

Target: Quest standalone Beat Saber **1.40.8_7379**. Experimental; not a
headset-certified fix for Dynasty's white view.

## What changed

- Configure the VideoPlayer on an inactive, renderer-free child GameObject
  before activating it. It still decodes to one owned 2D RenderTexture.
- Clear only Nexora's own renderer property block on first-frame binding.
  Material bindings alone cannot exclude per-renderer overrides.
- Neutral video takes one RGB texture sample. Authored effects still select
  the full effect path. Reversed-edge GLSL `smoothstep` was made well-defined.
- Optional `rawVideoDiagnostic: true` in Nexora's config bypasses fragment
  effects for A/B testing. Default is **false**. Restart Beat Saber after a
  config change; restore false after the comparison. It performs no readback.
- Optional RGBD uses one packed video/decoder/clock, not two independently
  seeking players. A shader variant samples depth on the GPU, displaces a
  192-by-96 mesh and writes scene depth when fully opaque. Ordinary RGB keeps
  its existing mesh resolution and has no vertex depth texture fetch.

## Authored media contract

The existing Dynasty MP4 is **RGB-only**. These changes do not create missing
depth and do not enable RGBD in Dynasty's DAT automatically.

RGBD requires a video exported from the *same capture position, projection,
frames and timebase* as its RGB imagery. Pack two panels horizontally into a
single frame. RGB is on the left; grayscale radial distance is on the right.
Each panel independently covers the same complete mono 360 panorama. The
depth panel can be narrower to reduce decoded pixels. Both panels have the
same height. Do not interpret the right panel as the right eye.

Depth code values encode `(radialDistanceMeters - nearMeters) / (farMeters -
nearMeters)`, clamped to 0..1. These are **radial distances**, not nonlinear
camera Z or luminance from the color video. Grayscale is stored in video RGB
code values; the shader undoes sRGB sampling on the depth channel in Linear
projects. Video compression/8-bit quantization still limits depth precision.
Use a tight useful near/far range, smooth depth boundaries, matching vertical
orientation, and a capture whose play area stays clear.

Example **only for a genuinely packed RGBD file**:

```json
{
  "b": 0,
  "t": "Nexora.LoadVideo",
  "d": {
    "id": "world",
    "media": "scene-rgbd.mp4",
    "projection": "mono",
    "followPlayer": false,
    "offset": [0, 1.6, 0],
    "rgbd": {
      "layout": "color-left-depth-right",
      "nearMeters": 2,
      "farMeters": 30,
      "colorWidth": 0.8
    }
  }
}
```

`offset` is the world-space capture origin; use the actual authored origin,
not blindly this example's 1.6m height. RGBD is world-anchored so head
translation produces parallax. `radius` no longer determines the surface;
depth does. Existing yaw/rotation/scale/deformation still transform the mesh.
SBS/OU and following the head are deliberately not accepted as RGBD layouts.

This is depth-reconstructed **2.5D**, not the complete scene geometry that
Vivify can instantiate. Newly revealed surfaces behind foreground objects
are absent from a single capture. It cannot guarantee artifact-free free
movement, identical Vivify visuals or identical GPU/frame-time costs.
One decoder avoids a second decode pipeline but does not make additional
pixels, vertices and depth writes free. Measure on the target Quest before
choosing production resolution/bitrate or enabling multiple RGBD layers.

## Evidence and limits (2026-09-09)

- Earlier Quest 0.3.3 pause probe: frame 3082, 51.365s; material and decoder
  target IDs matched. 320 sampled pixels ranged from RGB (9,52,14) to
  (153,202,255). This shows varied decoded content, not correct final drawing.
- 0.3.4 native code builds for Android AArch64. Unity 2021.3.16f1 built the
  Android bundle with GLES3 and Vulkan, including Multiview and RGBD variants.
- `Nexora.Editor.NexoraRenderingProbe.Run` creates a disposable preview scene.
  Metal GPU A/B: raw vs neutral sample error 0; original effect path neutral
  mean error 0; near depth occluded a test cube, far depth revealed it, and
  disabling depth writes revealed it. This tests actual shader rendering,
  **not** decoder behavior, XR eye routing or Quest performance.
- No 0.3.4 Quest run yet: the device was not visible in ADB during this build.

Unity documents [renderer property overrides](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/Renderer.SetPropertyBlock.html),
[VideoPlayer renderer targeting](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/Video.VideoPlayer-targetMaterialRenderer.html),
and [platform depth differences](https://docs.unity3d.com/2021.3/Documentation/Manual/SL-PlatformDifferences.html).
These support the implementation choices, not a claim that the white-view
root cause has been proven on the headset.
