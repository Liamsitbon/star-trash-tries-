#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
expected_version="0.6.10"
expected_qpm_config_version="0.4.0"

if [[ "${VIVIFY_SKIP_HOST_TESTS:-0}" == "1" ]]; then
  printf '%s\n' 'SKIP host lifecycle/performance tests (VIVIFY_SKIP_HOST_TESTS=1)'
else
  "$project_dir/scripts/test-port-foundation.sh"
fi

qpm_version="$(jq -er '.version' "$project_dir/qpm.json")"
qpm_info_version="$(jq -er '.info.version' "$project_dir/qpm.json")"
shared_version="$(jq -er '.config.version' "$project_dir/qpm.shared.json")"
shared_info_version="$(jq -er '.config.info.version' "$project_dir/qpm.shared.json")"
manifest_version="$(jq -er '.version' "$project_dir/mod.json")"
cmake_version="$(sed -n 's/^set(MOD_VERSION "\([^"]*\)")$/\1/p' "$project_dir/qpm_defines.cmake")"

for value in \
  "$qpm_info_version" \
  "$shared_info_version" \
  "$manifest_version" \
  "$cmake_version"; do
  if [[ "$value" != "$expected_version" ]]; then
    printf 'FAIL expected version %s, found %s\n' "$expected_version" "$value" >&2
    exit 1
  fi
done
if [[ "$qpm_version" != "$expected_qpm_config_version" ||
      "$shared_version" != "$expected_qpm_config_version" ]]; then
  printf 'FAIL expected QPM config format %s, found qpm=%s shared=%s\n' \
    "$expected_qpm_config_version" "$qpm_version" "$shared_version" >&2
  exit 1
fi
printf 'PASS mod metadata version consistency (%s) and QPM config format (%s)\n' \
  "$expected_version" "$expected_qpm_config_version"

events=(
  InstantiatePrefab
  DestroyObject
  SetMaterialProperty
  SetAnimatorProperty
  SetGlobalProperty
  AssignObjectPrefab
  Blit
  CreateCamera
  CreateScreenTexture
  SetCameraProperty
  SetRenderingSettings
)
for event_name in "${events[@]}"; do
  rg -q "= \"${event_name}\"sv;" "$project_dir/include/VivifyTypes.hpp"
done
printf 'PASS all %d Windows-master event identifiers are registered\n' "${#events[@]}"

required_patterns=(
  'VivifyLifecycle'
  'BeginCameraRender'
  'IsCameraApplierCurrent'
  'ApplySecondaryCameraProperties'
  'mainEffect.value_or(true)'
  'UpdateDynamicGI'
  'SaberTrailRendererEnabler'
  'RevealOriginalForAssignedPrefabs'
  'RenderCommandCacheGate'
  'AcquireImageBlitCommandBuffer'
  'StaggeredRefreshDelay'
  'MeaningfulRateChange'
  'StableSyncRate'
  'PrefabInitialElapsed'
  'legacyAnimations'
  'DefaultLegacyAnimationState'
  'needsInitialTimelineSample'
  'sampled legacy animation timeline'
  'BeatmapCallbacksController is the authoritative clock'
  'authored render settings preserved'
  'Vivify Quest perf window'
  '_sharedColorPropertyBlock'
  'collect-quest3-performance.ps1'
  'analyze-quest3-performance.ps1'
  'appliedFingerprint'
  '_activeNoteControllers'
  'noteRefreshCandidates'
  'QuestModInterop::InstalledPeers'
  'Vivify interop:'
  '_mainBundle->Unload(true)'
)
for pattern in "${required_patterns[@]}"; do
  rg -q -F "$pattern" "$project_dir/include" "$project_dir/src" "$project_dir/scripts"
done
printf '%s\n' 'PASS required lifecycle/parity implementation anchors are present'

note_refresh_body="$(sed -n '/^void Runtime::RefreshActiveNoteVisuals(/,/^void Runtime::CleanCustomObject(/p' "$project_dir/src/VivifyObjectPrefabs.cpp")"
if grep -q -F 'FindObjectsOfType' <<<"$note_refresh_body"; then
  printf '%s\n' 'FAIL AssignObjectPrefab refresh must not perform a scene-wide note scan' >&2
  exit 1
fi
for required_refresh_pattern in \
  '_activeNoteControllers' \
  'NoteMatchesTracks(affectedTracks' \
  'ApplyNotePrefabFor(noteController)'; do
  if ! grep -q -F "$required_refresh_pattern" <<<"$note_refresh_body"; then
    printf 'FAIL active-note refresh is missing %s\n' "$required_refresh_pattern" >&2
    exit 1
  fi
done
printf '%s\n' 'PASS Axo-style active-note refresh avoids global scans and targets affected tracks'

stereo_sync_body="$(sed -n '/if (textureArrayStereo)/,/} else if (!actualMultipass)/p' "$project_dir/src/VivifyComponents.cpp")"
for required_stereo_pattern in \
  'set_projectionMatrix(other->get_projectionMatrix())' \
  'set_worldToCameraMatrix(other->get_worldToCameraMatrix())' \
  'ResetCullingMatrix()'; do
  if ! grep -q -F "$required_stereo_pattern" <<<"$stereo_sync_body"; then
    printf 'FAIL texture-array stereo sync is missing %s\n' "$required_stereo_pattern" >&2
    exit 1
  fi
done
if grep -q -E -- '->SetStereo(Projection|View)Matrix|->set_cullingMatrix' <<<"$stereo_sync_body"; then
  printf '%s\n' 'FAIL texture-array stereo path must leave per-eye and culling matrices to XR' >&2
  exit 1
fi
printf '%s\n' 'PASS secondary cameras leave multiview eye matrices and culling to XR'

per_eye_blit_source="$project_dir/src/VivifyPostProcessing.cpp"
rg -q -F '_StereoActiveEye' "$per_eye_blit_source"
rg -q -F 'QueueStereoArrayCopy' "$per_eye_blit_source"
if ! rg -q -F 'QueueStereoMaterialBlit' "$per_eye_blit_source" ||
   ! rg -q -F 'Blit_Identifier' "$per_eye_blit_source" ||
   ! rg -q -F 'scale, offset, 0, 0' "$per_eye_blit_source" ||
   ! rg -q -F 'scale, offset, 1, 1' "$per_eye_blit_source"; then
  printf '%s\n' 'FAIL ordinary Quest material blits are missing the explicit depth-slice path' >&2
  exit 1
fi
if rg -q 'EnableKeyword\(u"VIVIFY_PER_EYE_MULTIPASS"' "$per_eye_blit_source"; then
  printf '%s\n' 'FAIL obsolete VIVIFY_PER_EYE_MULTIPASS keyword path is still active' >&2
  exit 1
fi
rg -q -F 'MULTIPASS_ENABLED is valid only for Unity' "$project_dir/src/VivifyAssets.cpp"
rg -q -F 'SetMaterialKeyword(material, u"VIVIFY_PER_EYE_MULTIPASS", false)' "$project_dir/src/VivifyAssets.cpp"
printf '%s\n' 'PASS ordinary multiview material blits use explicit Quest depth slices; multiview-only keyword is disabled'

pause_body="$(sed -n '/^void Runtime::SetPauseMenuActive(bool active)/,/^void Runtime::HandleScenesWillDismiss()/p' "$project_dir/src/VivifyCore.cpp")"
if grep -q -E 'RestoreRenderSettings\(|ReapplyCurrentRenderSettings\(' <<<"$pause_body"; then
  printf '%s\n' 'FAIL pause/resume must not mutate swapchain-affecting authored render settings' >&2
  exit 1
fi
printf '%s\n' 'PASS pause/resume preserves authored render settings across XR suspend'

for required_pause_pattern in \
  '_pauseMenuRequested || _applicationPaused || !_applicationFocused' \
  'animator->set_speed(0.0f)' \
  'SetLegacyAnimationSpeed(animation, 0.0f)' \
  'main.set_simulationSpeed(0.0f)' \
  'vp->Pause()' \
  '_lifecycle.Suspend()'; do
  if ! grep -q -F "$required_pause_pattern" <<<"$pause_body"; then
    printf 'FAIL unified pause path is missing %s\n' "$required_pause_pattern" >&2
    exit 1
  fi
done
freeze_line="$(grep -n -m1 -F 'animator->set_speed(0.0f)' <<<"$pause_body" | cut -d: -f1)"
suspend_line="$(grep -n -m1 -F '_lifecycle.Suspend()' <<<"$pause_body" | cut -d: -f1)"
if ((freeze_line >= suspend_line)); then
  printf '%s\n' 'FAIL synchronized Unity objects must freeze before lifecycle suspension' >&2
  exit 1
fi
for application_callback in 'SetApplicationPaused(paused)' 'SetApplicationFocused(focused)'; do
  rg -q -F "$application_callback" "$project_dir/src/VivifyComponents.cpp"
done
printf '%s\n' 'PASS menu, focus and application pause freeze synchronized objects before Update suspension'

rg -q -F 'MetaCore::Game::SetScoreSubmission("Vivify", true);' "$project_dir/src/VivifyAssets.cpp"
if rg -q -F 'MetaCore::Game::SetScoreSubmission("Vivify", false)' "$project_dir/src"; then
  printf '%s\n' 'FAIL Vivify still contains a score-submission disabling path' >&2
  exit 1
fi
printf '%s\n' 'PASS Vivify releases only its own MetaCore score-submission flag'

prefab_source="$project_dir/src/VivifyObjectPrefabs.cpp"
for required_prefab_pattern in \
  'currentSongTime - customEventData->time <= 1.0f' \
  'ForceGameObjectRenderersOnTop(noteController->get_gameObject().unsafePtr())' \
  'ForceGameObjectRenderersOnTop(smc->get_gameObject().unsafePtr())' \
  'ForceRendererOnTop(followedRenderer->____meshRenderer.unsafePtr())'; do
  if ! rg -q -F "$required_prefab_pattern" "$prefab_source"; then
    printf 'FAIL 42 Flux startup/readability path is missing %s\n' "$required_prefab_pattern" >&2
    exit 1
  fi
done
cache_body="$(sed -n '/^void Runtime::CacheReplacementRenderers(/,/^void Runtime::InstantiateReplacementPrefab(/p' "$prefab_source")"
if ! grep -q -F 'ForceRendererOnTop(renderer)' <<<"$cache_body"; then
  printf '%s\n' 'FAIL replacement renderers are not protected from foreground occlusion' >&2
  exit 1
fi
printf '%s\n' 'PASS negative-time opening prewarm and gameplay renderer-order protections are present'

jq -e '
  .id == "vivify" and
  .modloader == "Scotland2" and
  .packageVersion == "1.40.8_7379" and
  .lateModFiles == ["libVivify.so"]
' "$project_dir/mod.json" >/dev/null
printf '%s\n' 'PASS QMOD manifest target and payload declaration'
