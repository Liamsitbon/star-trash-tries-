# 0.5.23 verification checklist

Static checks performed before packaging:

- All manifests parse as JSON and report version 0.5.23.
- `_QPVersion` remains 1.2.0.
- Every feature toggle defaults to enabled.
- `UseQuestMultiviewBlitBypass()` follows the Multiview safety toggle and XR
  stereo mode; disabling safety returns to the full render path.
- Safe mode rejects culling-layer mutation and does not enable CameraApplier
  solely for secondary cameras.
- Pause/resume preserves each secondary camera's prior enabled state.
- Reset/restart can proactively prepare the current beatmap again.

A Quest runtime test is still required because this environment cannot execute
Beat Saber or the Android IL2CPP mod.
