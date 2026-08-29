# Vivify Quest 0.6.6: opt-in per-eye texture-array Blits

## What changed

Quest Multiview renders both eyes into a two-layer texture array. A normal
full-screen `CommandBuffer.Blit` does not guarantee that an authored Vivify
shader will draw and sample the correct layer separately for each eye. Vivify
0.6.6 therefore offers an explicit compatibility path for materials that opt in.

Add this material property and shader variant:

```hlsl
_VivifyPerEyeBlit ("Vivify per-eye Blit", Float) = 1
#pragma multi_compile _ VIVIFY_PER_EYE_MULTIPASS
```

When Quest is using texture-array stereo and both temporary targets are arrays,
Vivify issues one Blit to destination slice 0 and one to slice 1. The shader
samples its source array layer with `_StereoActiveEye`. Materials without the
marker continue through the existing single Blit path, so existing maps do not
pay the extra draw cost.

`MULTIPASS_ENABLED` remains reserved for Unity's real XR MultiPass rendering
mode. `VIVIFY_PER_EYE_MULTIPASS` means explicit per-eye compatibility draws
inside Multiview; it does not claim that the XR runtime changed rendering mode.

## Validation boundary

- Source inspection confirms the opt-in gate, array checks and two destination
  slices are used in immediate and mid-render Blit command buffers.
- The Android ARM64 compiler and QMOD integrity results are recorded in
  `BUILD_AND_TEST_0.6.6.md`.
- Headset stereoscopy is not proven by a host build. Test an opt-in effect while
  alternately closing each eye, then capture `Vivify.log` and Quest video.
