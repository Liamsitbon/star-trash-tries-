# Vivify Quest 0.5.26 build fix

The original 0.5.26 source used a six-argument `UnityEngine::Graphics::Blit` overload to select a source and destination texture-array slice. The generated Beat Saber 1.40.8 `bs-cordl` bindings expose only the 2-, 3-, and 4-argument Blit overloads, so Clang rejected both calls.

The per-eye transfer now uses the already-supported six-argument `Graphics::CopyTexture` binding:

- `TwoEyes[eye] -> mono[0]` before processing an eye.
- `mono[0] -> TwoEyes[eye]` after processing an eye.

The dimensions and format remain inherited from the source descriptor, while the mono temporary changes only VR usage, texture dimension, volume depth, MSAA, and depth-buffer allocation.

Before restoring dependencies, select the NDK required by `qpm.json`:

```bash
qpm ndk resolve --download
qpm restore
qpm s qmod
```
