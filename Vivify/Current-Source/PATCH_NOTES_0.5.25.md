# Vivify Quest 0.5.25 — filter-only Multiview safety

## Confirmed regression

With Multiview safety enabled, affected maps could render the left eye white and the right eye black. Disabling the setting removed the eye split. The previous safety implementation changed helper cameras to mono, changed RenderTexture VR layout, disabled the map's eye keyword, suppressed camera culling, and replaced the normal compositor with a transient mono path.

## Fix

Multiview safety is now an event filter only:

- It never changes `stereoTargetEye`, camera matrices, RenderTexture `vrUsage`, texture formats, culling, or `MULTIPASS_ENABLED`.
- It never substitutes the normal stereo compositor with a mono/transient compositor.
- It rejects only direct or chained effects known to fail on Quest Multiview, including Bloom, GaussianBlur, ChromaticAberration, KickWow, Vignette, PixelSort, Scram, Difference, MotionBlur, and ColorZoom.
- Safe direct effects continue through the same stereo path used when safety is disabled.
- Secondary/depth cameras and their authored culling remain active.

## Expected results

- No white-left/black-right eye split caused by the safety setting.
- Murder Plot's known KickWow/Blur/ChromaticAberration/Vignette chain is filtered instead of rendered as repeated rows.
- 42 Flux's unsafe Bloom pyramid is filtered, while safe camera effects and camera isolation remain available.
- Maps already working correctly, including You, Aether, and Overclocked, keep the same stereo renderer and shader state.
