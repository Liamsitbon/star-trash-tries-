#!/usr/bin/env bash
set -euo pipefail
# Read-only collection: no app restart, refresh-rate override or system settings.
NEXORA_METRICS_DIR="${1:?Supply a new local output directory}"
NEXORA_ADB="${ADB:-adb}"
if [[ -e "$NEXORA_METRICS_DIR" ]]; then
  echo "Use a new output directory; refusing to overwrite evidence." >&2; exit 2
fi
"$NEXORA_ADB" get-state >/dev/null
mkdir -p "$NEXORA_METRICS_DIR"
"$NEXORA_ADB" shell dumpsys meminfo com.beatgames.beatsaber > "$NEXORA_METRICS_DIR/meminfo.txt"
"$NEXORA_ADB" logcat -d -v threadtime > "$NEXORA_METRICS_DIR/logcat.log"
"$NEXORA_ADB" pull /sdcard/ModData/com.beatgames.beatsaber/Logs/Nexora.log "$NEXORA_METRICS_DIR/Nexora.log"
echo "Collected game memory and existing logs. GPU/CPU frame timing needs a matching OVR Metrics capture; absence is not zero."
