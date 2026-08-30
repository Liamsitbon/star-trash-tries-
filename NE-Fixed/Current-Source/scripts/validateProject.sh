#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

required_files=(
  CMakeLists.txt qpm.json qpm.shared.json qpm_defines.cmake mod.json cover.png README.md LICENSE
  src/main.cpp src/NoodleExtensions.cpp src/SpawnDataHelper.cpp
  include/NECaches.h include/NEHooks.h include/NEConfig.h include/NELogger.h include/QuestInterop.hpp
  scripts/build.sh scripts/build.ps1 scripts/buildQMOD.sh scripts/validateProject.sh
)

for file in "${required_files[@]}"; do
  if [[ ! -f "$file" ]]; then
    echo "Validation error: missing $file" >&2
    echo "Project root: $PROJECT_ROOT" >&2
    echo "Current src files:" >&2
    find "$PROJECT_ROOT/src" -maxdepth 2 -type f -print 2>/dev/null | sort >&2 || true
    exit 1
  fi
done

while IFS= read -r script; do
  bash -n "$script"
done < <(find scripts -maxdepth 1 -type f -name '*.sh' -print | sort)

crlf_files="$(find scripts -maxdepth 1 -type f -name '*.sh' -print0 | xargs -0 grep -Il $'\r' 2>/dev/null || true)"
if [[ -n "$crlf_files" ]]; then
  echo "Validation error: shell scripts use Windows CRLF line endings:" >&2
  printf '%s\n' "$crlf_files" >&2
  exit 1
fi

if find . -path './.git' -prune -o \( -name '.DS_Store' -o -name '__MACOSX' \) -print | grep -q .; then
  echo "Validation error: macOS metadata is present in the project" >&2
  find . -path './.git' -prune -o \( -name '.DS_Store' -o -name '__MACOSX' \) -print >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "Validation error: python3 is required for project metadata validation" >&2
  exit 1
fi

python3 - <<'PY'
import json
import pathlib
import re
import sys

root = pathlib.Path('.')

def fail(message):
    print(f'Validation error: {message}', file=sys.stderr)
    raise SystemExit(1)

def load_json(path):
    try:
        with path.open(encoding='utf-8') as f:
            return json.load(f)
    except Exception as exc:
        fail(f'{path} is not valid JSON: {exc}')

mod = load_json(root / 'mod.json')
qpm = load_json(root / 'qpm.json')
shared = load_json(root / 'qpm.shared.json')
cmake_text = (root / 'qpm_defines.cmake').read_text(encoding='utf-8')

match = re.search(r'set\(MOD_VERSION "([^"]+)"\)', cmake_text)
cmake_version = match.group(1) if match else None
versions = {
    'mod.json': mod.get('version'),
    'qpm.json': qpm.get('info', {}).get('version'),
    'qpm.shared.json': shared.get('config', {}).get('info', {}).get('version'),
    'qpm_defines.cmake': cmake_version,
}
if None in versions.values() or len(set(versions.values())) != 1:
    fail(f'version mismatch: {versions}')

ids = {
    'mod.json': mod.get('id'),
    'qpm.json': qpm.get('info', {}).get('id'),
    'qpm.shared.json': shared.get('config', {}).get('info', {}).get('id'),
}
if None in ids.values() or len(set(ids.values())) != 1:
    fail(f'mod ID mismatch: {ids}')

if mod.get('packageId') != 'com.beatgames.beatsaber':
    fail(f'unexpected packageId: {mod.get("packageId")!r}')
if mod.get('packageVersion') != '1.40.8_7379':
    fail(f'unexpected Beat Saber packageVersion: {mod.get("packageVersion")!r}')
if mod.get('modloader') != 'Scotland2':
    fail(f'unexpected modloader: {mod.get("modloader")!r}')

late_files = mod.get('lateModFiles') or []
override_so = qpm.get('info', {}).get('additionalData', {}).get('overrideSoName')
if override_so not in late_files:
    fail(f'QPM output {override_so!r} is not listed in mod.json lateModFiles')

for source in ('src/main.cpp', 'src/NoodleExtensions.cpp'):
    text = (root / source).read_text(encoding='utf-8')
    if source.endswith('main.cpp'):
        for symbol in ('setup', 'late_load'):
            pattern = rf'extern\s+"C"[^{{;]*\bvoid\s+{symbol}\s*\('
            if not re.search(pattern, text):
                fail(f'{source} is missing exported loader entry point {symbol!r}')

for metadata_name, dependencies in (
    ('mod.json', mod.get('dependencies', [])),
    ('qpm.json', qpm.get('dependencies', [])),
):
    dep_ids = [d.get('id') for d in dependencies]
    duplicates = sorted({x for x in dep_ids if dep_ids.count(x) > 1})
    if duplicates:
        fail(f'{metadata_name} has duplicate dependencies: {duplicates}')

required_dependencies = {'beatsaber-hook', 'custom-types', 'custom-json-data', 'tracks', 'songcore'}
qpm_dependencies = {d.get('id') for d in qpm.get('dependencies', [])}
missing = sorted(required_dependencies - qpm_dependencies)
if missing:
    fail(f'qpm.json is missing required dependencies: {missing}')

peer_dependencies = {'cinema', 'noodleextensions', 'nexora', 'vivify'}
hard_peers = sorted(peer_dependencies & {str(value).casefold() for value in qpm_dependencies})
if hard_peers:
    fail(f'qpm.json has forbidden hard peer-mod dependencies: {hard_peers}')

license_text = (root / 'LICENSE').read_text(encoding='utf-8')
if 'MIT License' not in license_text or 'Copyright (c) 2020 Aeroluna' not in license_text:
    fail('original Noodle Extensions MIT license/attribution is missing')

build_script = qpm.get('workspace', {}).get('scripts', {}).get('build', [])
if not any('scripts/build.sh' in command for command in build_script):
    fail('qpm build script does not use scripts/build.sh')

generated_or_local_dirs = {'.git', 'build', 'extern', 'shared'}
local_only_files = {'ndkpath.txt', 'ndkpath.example.txt'}

for path in root.rglob('*'):
    if generated_or_local_dirs.intersection(path.parts) or not path.is_file():
        continue
    if path.name in local_only_files:
        continue
    if path.suffix.lower() in {'.png', '.jpg', '.jpeg', '.zip', '.qmod', '.so', '.a'}:
        continue
    try:
        text = path.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        continue
    private_path_patterns = (
        re.compile(r'/Users/[^/]+/(?:Library|Desktop|Documents|Downloads)/'),
        re.compile(r'[A-Za-z]:\\Users\\[^\\]+\\(?:AppData|Desktop|Documents|Downloads)\\'),
    )
    if any(pattern.search(text) for pattern in private_path_patterns):
        fail(f'personal absolute path leaked into {path}')

print(f'Project validation passed: {ids["mod.json"]} {versions["mod.json"]}')
PY

grep -q -F 'QuestModInterop::Inspect' src/NoodleExtensions.cpp
grep -q -F 'NECaches::VivifyActive' src/Hooks/SceneTransition/SceneTransitionHelper.cpp
grep -q -F 'NECaches::NexoraActive' src/Hooks/SceneTransition/SceneTransitionHelper.cpp
grep -q -F 'NECaches::CinemaActive' src/Hooks/SceneTransition/SceneTransitionHelper.cpp

transform_source="src/Hooks/BeatmapDataTransformHelper.cpp"
late_source="src/Hooks/FakeNotes/BeatmapData.cpp"
associated_source="src/AssociatedData.cpp"
fake_helper_source="src/Hooks/FakeNotes/FakeNoteHelper.cpp"
movement_source="src/Animation/NoodleMovementDataProvider.cpp"
spawn_helper="include/SpawnDataHelper.h"
note_source="src/Hooks/NoteController.cpp"

for fake_array in fakeColorNotes fakeBombNotes fakeObstacles fakeBurstSliders fakeSliders; do
  if ! grep -q -F "INJECT_V3_FAKE_ARRAY(${fake_array}" "$fake_helper_source"; then
    echo "Validation error: shared V3 fake-object injector is missing ${fake_array}" >&2
    exit 1
  fi
done
for required_fake_pattern in \
  'customDataIt->value.AddMember(' \
  'customDataIt->value.SetObject()' \
  'NoodleExtensions::Constants::INTERNAL_FAKE_NOTE.data()' \
  'NE_fakeObjectsInjected' \
  '(array)->Add(item)' \
  'EnsureV3FakeObjectsInjected'; do
  if ! grep -q -F "$required_fake_pattern" "$fake_helper_source"; then
    echo "Validation error: shared V3 fake-object injector is missing ${required_fake_pattern}" >&2
    exit 1
  fi
done

grep -q -F 'FakeNoteHelper::EnsureV3FakeObjectsInjected(beatmap)' "$transform_source"
grep -q -F 'preInjectedBeforeLoad' "$late_source"
grep -q -F 'EnsureV3FakeObjectsInjected(customSaveData)' "$late_source"
grep -q -F 'late fallback skipped' "$late_source"
grep -q -F 'ScoringType::NoScore' "$associated_source"
grep -q -F 'fake.value_or(false) || ad.objectData.uninteractable.value_or(false)' "$fake_helper_source"
grep -q -F 'return !ad.objectData.uninteractable.value_or(false)' "$fake_helper_source"

if grep -q -F 'intersectingObstacles->Remove(obstacle)' "$fake_helper_source"; then
  echo 'Validation error: fake-obstacle filter mutates its source list while iterating' >&2
  exit 1
fi
grep -q -F 'filtered->Add(obstacle)' "$fake_helper_source"

grep -q -F 'auto valueType = noteJumpSpeedOverride.has_value() || objectOffset.has_value()' "$movement_source"
grep -q -F '!inputNjs.has_value() && !inputOffset.has_value()' "$spawn_helper"

if grep -q -F 'if (!isFakeNote)' "$note_source"; then
  echo 'Validation error: authored note interactable animation is still force-enabled for real notes' >&2
  exit 1
fi
grep -q -F 'if (ad.objectData.uninteractable.value_or(false)) enabled = false;' "$note_source"

echo 'Fake-object validation passed: cached-song V3 pre-injection, duplicate guard, safe filtering and NoScore semantics'
echo 'Parity validation passed: per-object jump timing and authored note interactability'
exit 0
bash scripts/buildQMOD.sh
