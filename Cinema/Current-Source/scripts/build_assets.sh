#!/usr/bin/env bash
set -euo pipefail

CINEMA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CINEMA_UNITY_BIN="${CINEMA_UNITY_BIN:-/Applications/Unity/Hub/Editor/2021.3.16f1/Unity.app/Contents/MacOS/Unity}"
CINEMA_LOG="$CINEMA_ROOT/release/unity-assets.log"
CINEMA_BUNDLE="$CINEMA_ROOT/assets/cinemaassets.android"

if [[ ! -x "$CINEMA_UNITY_BIN" ]]; then
  echo "Unity 2021.3.16f1 was not found. Set CINEMA_UNITY_BIN." >&2
  exit 2
fi

mkdir -p "$CINEMA_ROOT/release" "$CINEMA_ROOT/assets"
"$CINEMA_UNITY_BIN" \
  -batchmode -nographics -quit \
  -buildTarget Android \
  -projectPath "$CINEMA_ROOT/unity" \
  -executeMethod CinemaQuest.Editor.BuildCinemaAssets.BuildAndroid \
  -logFile "$CINEMA_LOG"

if [[ ! -s "$CINEMA_BUNDLE" ]] ||
   ! grep -q "CINEMA_QUEST_ASSET_BUNDLE_OK" "$CINEMA_LOG"; then
  echo "Unity did not produce a verified Cinema Quest Android bundle." >&2
  grep -E "Access token is unavailable|Failed to activate|Shader error|error CS|Exception" \
      "$CINEMA_LOG" | tail -20 >&2 || true
  exit 3
fi

shasum -a 256 "$CINEMA_BUNDLE"
