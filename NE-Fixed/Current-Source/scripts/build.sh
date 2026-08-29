#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_TYPE="RelWithDebInfo"
CLEAN=0
CONFIGURE_ONLY=0
NDK_OVERRIDE=""

usage() {
  cat <<'USAGE'
Usage: ./scripts/build.sh [--release|--debug] [--clean] [--configure-only] [--ndk PATH]

Builds NoodleExtensions for Quest on macOS/Linux (and Windows through Git Bash).
NDK lookup order: --ndk, environment variables, ./ndkpath.txt, QPM-RS cache.
USAGE
}

while (($#)); do
  case "$1" in
    --release) BUILD_TYPE="RelWithDebInfo" ;;
    --debug) BUILD_TYPE="Debug" ;;
    --clean|-clean) CLEAN=1 ;;
    --configure-only) CONFIGURE_ONLY=1 ;;
    --ndk)
      shift
      [[ $# -gt 0 ]] || { echo "Error: --ndk requires a path" >&2; exit 2; }
      NDK_OVERRIDE="$1"
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

find_ndk() {
  local candidate=""

  for candidate in \
    "$NDK_OVERRIDE" \
    "${ANDROID_NDK_HOME:-}" \
    "${ANDROID_NDK_ROOT:-}" \
    "${ANDROID_NDK_LATEST_HOME:-}"; do
    if [[ -n "$candidate" && -f "$candidate/build/cmake/android.toolchain.cmake" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  if [[ -f "$PROJECT_ROOT/ndkpath.txt" ]]; then
    candidate="$(grep -vE '^[[:space:]]*(#|$)' "$PROJECT_ROOT/ndkpath.txt" | head -n 1 || true)"
    if [[ -n "$candidate" && -f "$candidate/build/cmake/android.toolchain.cmake" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  fi

  local roots=(
    "$HOME/Library/Application Support/QPM-RS/ndk"
    "${XDG_DATA_HOME:-$HOME/.local/share}/QPM-RS/ndk"
    "${APPDATA:-}/QPM-RS/ndk"
  )
  local root
  for root in "${roots[@]}"; do
    [[ -n "$root" && -d "$root" ]] || continue
    while IFS= read -r candidate; do
      [[ -f "$candidate/build/cmake/android.toolchain.cmake" ]] || continue
      printf '%s\n' "$candidate"
      return 0
    done < <(find "$root" -mindepth 1 -maxdepth 1 -type d -print 2>/dev/null | sort -r)
  done

  return 1
}

for command_name in cmake ninja; do
  command -v "$command_name" >/dev/null 2>&1 || {
    echo "Error: $command_name is not installed or not in PATH." >&2
    exit 1
  }
done

required_headers=(
  "extern/includes/beatsaber-hook/shared/utils/hooking.hpp"
  "extern/includes/bs-cordl/include"
  "extern/includes/scotland2/shared/loader.hpp"
)
missing_dependencies=0
for header in "${required_headers[@]}"; do
  [[ -e "$header" ]] || { missing_dependencies=1; break; }
done

if ((missing_dependencies)); then
  if command -v qpm-rust >/dev/null 2>&1; then
    echo "Dependencies are missing; running qpm-rust restore..."
    qpm-rust restore
    qpm-rust cache legacy-fix || true
  elif command -v qpm >/dev/null 2>&1; then
    echo "Dependencies are missing; running qpm restore..."
    qpm restore
    qpm cache legacy-fix || true
  else
    echo "Error: required dependency headers are missing and neither qpm-rust nor qpm is available." >&2
    exit 1
  fi
fi

NDK_PATH="$(find_ndk || true)"
if [[ -z "$NDK_PATH" ]]; then
  cat >&2 <<'ERROR'
Error: Android NDK was not found.
Set ANDROID_NDK_HOME, pass --ndk PATH, or copy ndkpath.example.txt to ndkpath.txt.
The project expects the NDK version selected by qpm.json (27.3.13750724 preview-compatible).
ERROR
  exit 1
fi

if ((CLEAN)); then
  rm -rf build
fi

mkdir -p build

echo "Building NoodleExtensions (${BUILD_TYPE})"
echo "Using NDK: ${NDK_PATH}"

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_ANDROID_NDK="$NDK_PATH"

if ((CONFIGURE_ONLY)); then
  echo "CMake configure completed."
  exit 0
fi

cmake --build build --parallel

echo "Build completed: $PROJECT_ROOT/build/libnoodleextensions.so"
