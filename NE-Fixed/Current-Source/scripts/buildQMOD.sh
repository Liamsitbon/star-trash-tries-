#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD_LIB="$PROJECT_ROOT/build/libnoodleextensions.so"
OUTPUT="$PROJECT_ROOT/NoodleExtensions.qmod"

cd "$PROJECT_ROOT"

for command_name in zip unzip; do
  command -v "$command_name" >/dev/null 2>&1 || {
    echo "Error: $command_name is not installed or not in PATH." >&2
    exit 1
  }
done

bash "$SCRIPT_DIR/validateProject.sh"

if [[ ! -f "$BUILD_LIB" ]]; then
  echo "build/libnoodleextensions.so was not found; building first..."
  qpm s build
fi

if [[ ! -f "$BUILD_LIB" ]]; then
  echo "Error: $BUILD_LIB was not produced." >&2
  exit 1
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/noodle-qmod.XXXXXX")"
cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

cp "$PROJECT_ROOT/mod.json" "$TMP_DIR/mod.json"
cp "$PROJECT_ROOT/cover.png" "$TMP_DIR/cover.png"
cp "$BUILD_LIB" "$TMP_DIR/libnoodleextensions.so"

rm -f "$OUTPUT"
(
  cd "$TMP_DIR"
  zip -q -9 -X "$OUTPUT" mod.json cover.png libnoodleextensions.so
)

unzip -tq "$OUTPUT" >/dev/null

if command -v shasum >/dev/null 2>&1; then
  CHECKSUM="$(shasum -a 256 "$OUTPUT" | awk '{print $1}')"
elif command -v sha256sum >/dev/null 2>&1; then
  CHECKSUM="$(sha256sum "$OUTPUT" | awk '{print $1}')"
else
  CHECKSUM="unavailable"
fi

SIZE="$(du -h "$OUTPUT" | awk '{print $1}')"
echo "Created: $OUTPUT"
echo "Size: $SIZE"
echo "SHA-256: $CHECKSUM"
echo "Archive contents:"
unzip -l "$OUTPUT" | sed -n '/mod.json/,$p'
