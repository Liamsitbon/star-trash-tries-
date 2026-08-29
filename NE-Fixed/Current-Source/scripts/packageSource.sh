#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

command -v zip >/dev/null 2>&1 || { echo "Error: zip is not installed." >&2; exit 1; }

bash "$SCRIPT_DIR/validateProject.sh"

VERSION="$(python3 - <<'PY'
import json
with open('mod.json', encoding='utf-8') as f:
    print(json.load(f)['version'])
PY
)"
OUTPUT_ARG="${1:-$PROJECT_ROOT/Noodle-Extensions-Quest-${VERSION}-Source.zip}"
if [[ "$OUTPUT_ARG" = /* ]]; then
  OUTPUT="$OUTPUT_ARG"
else
  OUTPUT="$PROJECT_ROOT/$OUTPUT_ARG"
fi
if [[ ! -d "$(dirname "$OUTPUT")" ]]; then
  echo "Error: output directory does not exist: $(dirname "$OUTPUT")" >&2
  exit 1
fi
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/noodle-source.XXXXXX")"
cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

ROOT_NAME="Noodle-Extensions-Quest-${VERSION}"
DEST="$TMP_DIR/$ROOT_NAME"
mkdir -p "$DEST"

# Copy only source-controlled/project files. Never include local NDK paths,
# dependencies, build products, logs, macOS metadata, or repository internals.
while IFS= read -r -d '' path; do
  relative="${path#./}"
  case "$relative" in
    .git/*|build/*|extern/*|shared/*|ndkpath.txt|scripts/ndkpath.txt|*.qmod|*.zip|.DS_Store|*/.DS_Store|__MACOSX/*)
      continue
      ;;
  esac
  mkdir -p "$DEST/$(dirname "$relative")"
  cp -p "$path" "$DEST/$relative"
done < <(find . -type f -print0)

rm -f "$OUTPUT"
(
  cd "$TMP_DIR"
  zip -q -9 -X -r "$OUTPUT" "$ROOT_NAME"
)

if command -v unzip >/dev/null 2>&1; then
  unzip -tq "$OUTPUT" >/dev/null
fi

if command -v shasum >/dev/null 2>&1; then
  CHECKSUM="$(shasum -a 256 "$OUTPUT" | awk '{print $1}')"
elif command -v sha256sum >/dev/null 2>&1; then
  CHECKSUM="$(sha256sum "$OUTPUT" | awk '{print $1}')"
else
  CHECKSUM="unavailable"
fi

echo "Created source archive: $OUTPUT"
echo "SHA-256: $CHECKSUM"
