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

"$NEXORA_HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I"$NEXORA_ROOT/include" \
  "$NEXORA_ROOT/tests/video_pixel_stats_test.cpp" \
  -o "$NEXORA_TEST_DIR/video_pixel_stats_test"
"$NEXORA_TEST_DIR/video_pixel_stats_test"
echo "Nexora bounded video pixel statistics test passed"
python3 "$NEXORA_ROOT/tests/test_video_log.py"

"$NEXORA_HOST_CXX" -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I"$NEXORA_ROOT/include" "$NEXORA_ROOT/tests/rgbd_video_test.cpp" \
  -o "$NEXORA_TEST_DIR/rgbd_video_test"
"$NEXORA_TEST_DIR/rgbd_video_test"
echo "Nexora opt-in RGBD layout test passed"

"$NEXORA_HOST_CXX" -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I"$NEXORA_ROOT/include" "$NEXORA_ROOT/tests/video_render_policy_test.cpp" \
  -o "$NEXORA_TEST_DIR/video_render_policy_test"
"$NEXORA_TEST_DIR/video_render_policy_test"
echo "Nexora direct-video default and diagnostic override policy test passed"

"$NEXORA_HOST_CXX" -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I"$NEXORA_ROOT/include" "$NEXORA_ROOT/tests/depth_projection_test.cpp" \
  -o "$NEXORA_TEST_DIR/depth_projection_test"
"$NEXORA_TEST_DIR/depth_projection_test"
echo "Nexora 2 region depth, quality and capture-pose tests passed"
