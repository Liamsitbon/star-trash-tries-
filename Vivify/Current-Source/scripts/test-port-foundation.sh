#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/vivify-port-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
test_binary="$test_dir/vivify-lifecycle-tests"
performance_test_binary="$test_dir/vivify-quest-performance-tests"

"${CXX:-c++}" \
  -std=c++20 \
  -Wall \
  -Wextra \
  -Werror \
  -I"$project_dir/include" \
  "$project_dir/src/VivifyLifecycle.cpp" \
  "$project_dir/tests/VivifyLifecycleTests.cpp" \
  -o "$test_binary"

"$test_binary"
printf '%s\n' 'PASS lifecycle state-machine test'

"${CXX:-c++}" \
  -std=c++20 \
  -Wall \
  -Wextra \
  -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -I"$project_dir/include" \
  "$project_dir/tests/VivifyQuestPerformanceTests.cpp" \
  -o "$performance_test_binary"

"$performance_test_binary"
printf '%s\n' 'PASS Quest performance policy tests (ASan/UBSan)'

pwsh -NoProfile -File "$project_dir/tests/Quest3PerformanceAnalysisTests.ps1"
printf '%s\n' 'PASS Quest performance capture analyzer test'
