# Vivify Quest 0.6.4 — authentic Axo fix audit and per-eye fix

## Scope and trust boundary

The original `https://github.com/axo-lotl/Vivify-Quest` repository is deleted.
The current public holder of some old Git objects is a different account and
is not trusted as the author of new fixes. The retained immutable commit
`b1401dd57197eab7b4e5d070c2727244669da2dc` is still resolved by GitHub's API
to the real `axo-lotl` account, user ID `281496494`; its direct parent
`9a5d54c82d1596d0b960f89dda0c66a616ea6460` is a GitHub-verified commit by the
same account. Current commits authored by the other account were excluded.

The authentic history contains one source-fix release after the 0.4 baseline:
Vivify Quest 0.4.1 (`b1401dd`). README, manifest, validation-script and workflow
commits do not add runtime fixes.

## Complete 0.4.1 source-fix comparison

| Authentic Axo change | 0.6.3/0.6.4 status |
| --- | --- |
| Move tracked render objects to culling layer 22 instead of toggling `Renderer.enabled` | Present, with cached/staggered renderer discovery and transition-safe restoration |
| Restore moved secondary-camera layers before the main camera culls | Present in `CameraApplier::OnPreCull` and runtime reset/release paths |
| Centralize note prefab application in all note-init hooks | Present |
| Fingerprint note and saber assignments | Present with fixed-width 64-bit fingerprints |
| Reuse intact replacements and reassert hidden originals | Present |
| Refresh known active notes after a relevant assignment | Present and strengthened in 0.6.3 with a registered controller pool plus affected-track filtering |
| Keep saber replacement nodes attached to the correct model transform | Present with additional fallback/integrity handling |
| Add `realtimeReflectionProbes` rendering-setting support | Present, including save/restore |

Result: there is no additional authentic Axo runtime fix missing from 0.6.4.
No code from the impersonating account's later commits was imported.

## Aether 0.6.3 hardware result

The user completed the Aether bridge test on a Quest 3S and reported that the
stall fix worked. The captured runtime begins with Vivify 0.6.3 and its loaded
library hash matched the locally packaged 0.6.3 library. The relevant
performance window processed 1,162 note-refresh events and selected 1,068
active candidates without reintroducing a scene-wide note scan.

This is recorded as a user-confirmed hardware pass for the reported Aether
stall, not as an independent visual certification of every Aether scene.
42 Flux is intentionally outside that acceptance result.

Evidence folder:

`diagnostics/2026-08-12-1029-0.6.3-aether-pass-per-eye-audit`

Vivify log SHA-256:

`dc52bcf2ed28797b2bbad9b0f26d338f8b29722b23291612696b3c0a72a16679`

## Per-eye failure diagnosis

The captured 42 Flux session runs XR stereo mode 3 (Quest Multiview). Its
`NotesOneCam` and `NotesCam` sources and owned color/depth outputs are already
correct 2016x2112 `Tex2DArray` textures with two slices and `TwoEyes` usage.
The texture layout is therefore not a mono allocation accidentally sent to
one eye.

The remaining fault was in `SecondaryCameraController::OnPreCull`. Both the
Windows master and the older Quest base copied only `projectionMatrix` and
`worldToCameraMatrix`, then forced `cullingMatrix` from those generic matrices.
On Multiview, however, a single render pass contains two eye-specific views
and projections. Reusing one generic/centre-eye culling matrix can make
secondary-camera content disagree between the two array slices.

## 0.6.4 implementation

For Quest texture-array stereo modes (single-pass instanced and Multiview):

1. Read the XR SDK projection matrix for the left eye and right eye separately.
2. Read the XR SDK view matrix for the left eye and right eye separately.
3. Apply all four matrices to the Vivify secondary camera every pre-cull.
4. Reset the old custom mono culling matrix so Unity chooses stereo-aware shared
   or per-eye culling from those views.
5. Keep the existing two-slice color/depth capture and mono-depth duplication
   fallback.
6. Log a bounded `secondary stereo sync` diagnostic with the left/right matrix
   deltas when debug logging is enabled.

This does not copy the left-eye image into the right eye. Both eyes render the
same authored scene and effects, but retain their own camera pose and
projection, so headset depth is preserved.

## Evidence boundary

The source check and ARM64 build prove that the matrix synchronization is
compiled and packaged. Only a physical headset test can prove that the
intermittent one-eye mismatch is gone for a particular map and shader bundle.
If a later map still differs between eyes while the new sync log is present,
the next suspect is that map's custom shader variant rather than the secondary
camera texture/matrix path.
