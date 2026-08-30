#!/usr/bin/env bash
set -euo pipefail

CINEMA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$CINEMA_ROOT/../.." && pwd)"
CINEMA_JOBS="${CINEMA_BUILD_JOBS:-6}"

# Refuse stale Unity VideoPlayer code before compiling. This check is read-only:
# the reviewed source is exactly the source CMake receives.
python3 "$REPO_ROOT/tools/apply_quest_runtime_fixes.py" --component cinema --verify-only

cd "$CINEMA_ROOT"
if [[ ! -d extern/includes || ! -d extern/libs ]]; then
  command -v qpm >/dev/null 2>&1 || {
    echo "qpm is required to restore Quest dependencies." >&2
    exit 2
  }
  qpm restore
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel "$CINEMA_JOBS"
file build/libCinema.so
