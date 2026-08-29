#!/usr/bin/env bash
set -euo pipefail

NEXORA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NEXORA_UNITY_BIN="${NEXORA_UNITY_BIN:-/Applications/Unity/Hub/Editor/2021.3.16f1/Unity.app/Contents/MacOS/Unity}"
NEXORA_LOG="$NEXORA_ROOT/release/unity-assets.log"
NEXORA_BUNDLE="$NEXORA_ROOT/assets/nexoraassets.android"
NEXORA_ALLOW_INTERACTIVE_LICENSE_FALLBACK="${NEXORA_ALLOW_INTERACTIVE_LICENSE_FALLBACK:-0}"

if [[ ! -x "$NEXORA_UNITY_BIN" ]]; then
  echo "Unity 2021.3.16f1 was not found. Set NEXORA_UNITY_BIN to the Unity executable." >&2
  exit 2
fi

mkdir -p "$NEXORA_ROOT/release"
NEXORA_BACKUP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/nexora-assets.XXXXXX")"
NEXORA_BUNDLE_BACKUP="$NEXORA_BACKUP_DIR/nexoraassets.android"
cleanup() { rm -rf "$NEXORA_BACKUP_DIR"; }
trap cleanup EXIT

# A Unity activation, shader, or importer failure must not destroy the last
# verified Quest bundle. Preserve it until a new Android build has passed the
# builder's shader/asset contract.
if [[ -s "$NEXORA_BUNDLE" ]]; then
  cp "$NEXORA_BUNDLE" "$NEXORA_BUNDLE_BACKUP"
fi
rm -f "$NEXORA_BUNDLE"
set +e
"$NEXORA_UNITY_BIN" \
  -batchmode -nographics -quit \
  -buildTarget Android \
  -projectPath "$NEXORA_ROOT/unity" \
  -executeMethod Nexora.Editor.BuildNexoraAssets.BuildAndroid \
  -logFile "$NEXORA_LOG"
NEXORA_UNITY_EXIT=$?
set -e

# After a restored Mac, Unity 2021 can fail entitlement refresh before project
# load in batch mode even when Hub has a valid Personal license. A normal
# editor launch refreshes that token after project load, while -quit still
# makes the operation unattended. Keep the fallback macOS-only and narrowly
# gated on the exact licensing failure so CI and real shader/build failures do
# not unexpectedly launch a GUI process.
if [[ $NEXORA_UNITY_EXIT -ne 0 \
      && "$NEXORA_ALLOW_INTERACTIVE_LICENSE_FALLBACK" == "1" \
      && "$(uname -s)" == "Darwin" \
      && -f "$NEXORA_LOG" ]] \
      && grep -q "Access token is unavailable" "$NEXORA_LOG"; then
  echo "Unity batch licensing failed; retrying through a normal editor launch." >&2
  rm -f "$NEXORA_BUNDLE"
  set +e
  "$NEXORA_UNITY_BIN" \
    -quit \
    -buildTarget Android \
    -projectPath "$NEXORA_ROOT/unity" \
    -executeMethod Nexora.Editor.BuildNexoraAssets.BuildAndroid \
    -logFile "$NEXORA_LOG"
  NEXORA_UNITY_EXIT=$?
  set -e
fi

if [[ $NEXORA_UNITY_EXIT -ne 0 || ! -s "$NEXORA_BUNDLE" ]] || ! grep -q "NEXORA_ASSET_BUNDLE_OK" "$NEXORA_LOG"; then
  if [[ -s "$NEXORA_BUNDLE_BACKUP" ]]; then
    cp "$NEXORA_BUNDLE_BACKUP" "$NEXORA_BUNDLE"
    echo "Restored the previous verified Nexora Android bundle after Unity failure." >&2
  else
    rm -f "$NEXORA_BUNDLE"
  fi
  echo "Unity did not produce a verified Nexora Android bundle (exit $NEXORA_UNITY_EXIT). See $NEXORA_LOG" >&2
  grep -E "Access token is unavailable|Failed to activate|Shader error|error CS|Exception" "$NEXORA_LOG" | tail -20 >&2 || true
  [[ $NEXORA_UNITY_EXIT -ne 0 ]] && exit "$NEXORA_UNITY_EXIT"
  exit 3
fi

shasum -a 256 "$NEXORA_BUNDLE"
