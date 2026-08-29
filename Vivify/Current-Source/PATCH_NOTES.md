# Vivify Quest 0.5.30

See `PATCH_NOTES_0.5.30.md` for synchronous render-command cleanup when the
gameplay camera is disabled during completion, retry, or menu transitions.

See `PATCH_NOTES_0.5.29.md` for Multiview stereo-slot/depth normalization,
idempotent culling caches, and the post-level MenuMainCamera crash fix.

See `PATCH_NOTES_0.5.28.md` for the original targetless per-eye secondary-camera capture,
first-play saber assignment, culling cost reduction, and the menu-exit crash
fix.

# Vivify Quest 0.5.27

See `PATCH_NOTES_0.5.27.md` for the native Multiview, restart, asset, shader,
saber, and overlay stability changes.

# Vivify Quest 0.5.26

See `PATCH_NOTES_0.5.26.md` for the retired per-eye Multiview compositor.

# Vivify Quest 0.5.25

See `PATCH_NOTES_0.5.25.md` for the filter-only Multiview safety rewrite.

# Vivify Quest 0.5.24

Current changes are documented in:

- `PATCH_NOTES_0.5.24.md` — native transition cleanup, transient Multiview Bloom, camera isolation, and note replacement.
- `STABILITY_TESTS_0.5.24.md` — Quest regression checklist.
- `BUILD_AND_TEST_0.5.24.md` — build and installation check.

The source retains the 0.5.21–0.5.23 notes only for the immediately preceding fixes that 0.5.24 builds on.


## Build compatibility correction

The source package now uses `Graphics::CopyTexture` for per-eye slice extraction and write-back. This matches the Beat Saber 1.40.8 bs-cordl bindings and fixes the two six-argument `Graphics::Blit` compilation errors.
