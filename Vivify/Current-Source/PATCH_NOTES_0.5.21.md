# Vivify Quest 0.5.21 — verified full-quality defaults and Multiview safety toggle

- All visible feature toggles default to enabled: Blit effects, beat-0 film grain, secondary/depth cameras, prefab prewarming, and Multiview safety.
- Added an independent `Multiview safety` toggle. Turning it off disables both `GetStereoSafetyMode()` and `UseQuestMultiviewSafePath()` instead of leaving a hidden safety fallback active.
- Verified every positive `enable...` setting maps directly to its runtime behavior; the internal `GetDisable...` helpers invert it exactly once.
- Full-quality rendering remains the default.
