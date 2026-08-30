#!/usr/bin/env bash
set -euo pipefail

NEXORA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$NEXORA_ROOT/../.." && pwd)"
NEXORA_JOBS="${NEXORA_BUILD_JOBS:-6}"

# Apply the audited Android video startup + decoder texture fixes before compile.
python3 "$REPO_ROOT/tools/apply_quest_runtime_fixes.py"

cd "$NEXORA_ROOT"
if [[ ! -d extern/includes || ! -d extern/libs ]]; then
  command -v qpm >/dev/null 2>&1 || {
    echo "qpm is required to restore Quest dependencies." >&2
    exit 2
  }
  qpm restore
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel "$NEXORA_JOBS"
file build/libNexora.so
