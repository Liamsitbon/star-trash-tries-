#!/usr/bin/env bash
set -euo pipefail
NE_ADDON_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$NE_ADDON_ROOT"
if [[ ! -f qpm_defines.cmake || ! -d extern/includes || ! -d extern/libs ]]; then
  qpm restore
fi
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel "${NE_ADDON_BUILD_JOBS:-3}"
file build/libNEFixed.so
