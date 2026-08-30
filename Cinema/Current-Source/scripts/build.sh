#!/usr/bin/env bash
set -euo pipefail

CINEMA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$CINEMA_ROOT/../.." && pwd)"
CINEMA_JOBS="${CINEMA_BUILD_JOBS:-6}"

# Keep the large upstream-derived runtime source readable while applying the
# audited Quest-only 0:00 decoder and Nexora black-screen deltas before build.
python3 "$REPO_ROOT/tools/apply_quest_runtime_fixes.py"

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
