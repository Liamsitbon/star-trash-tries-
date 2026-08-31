#!/usr/bin/env bash
set -euo pipefail

NEXORA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NEXORA_HOST_CXX="${NEXORA_HOST_CXX:-c++}"
NEXORA_TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/nexora-host-test.XXXXXX")"
NEXORA_TEST_BINARY="$NEXORA_TEST_DIR/runtime_lifecycle_test"
cleanup_host_test() { rm -rf "$NEXORA_TEST_DIR"; }
trap cleanup_host_test EXIT

"$NEXORA_HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I"$NEXORA_ROOT/include" \
  "$NEXORA_ROOT/src/NexoraLifecycle.cpp" \
  "$NEXORA_ROOT/tests/runtime_lifecycle_test.cpp" \
  -o "$NEXORA_TEST_BINARY"
"$NEXORA_TEST_BINARY"
echo "Nexora host lifecycle test passed"
