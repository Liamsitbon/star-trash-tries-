# Vivify Quest 0.5.30 — deterministic render cleanup

## Result/menu transition hardening

`CameraApplier` now removes its active mid-render command buffers immediately
from `OnDisable`. Unity can disable the gameplay camera on level completion,
retry, or exit without issuing the matching `OnPostRender` callback. Without
this cleanup, a command buffer can retain an authored map material while Unity
is already constructing the result or menu camera, matching the captured
`RenderingCommandBuffer::AddBlitRenderTarget` crash signature.

The existing 0.5.29 protections remain in place: transition-safe runtime reset,
no gameplay applier on `MenuMainCamera`, stereo-array secondary color/depth
capture, and no-op culling refreshes when authored data has not changed.

## Release boundary

Source/build validation cannot prove headset rendering parity. Before broad
distribution, run the Flux, Aether, Murder Plot, retry, completion, and menu
exit cases in `STABILITY_TESTS_0.5.30.md` and preserve the Vivify/Noodle logs.
