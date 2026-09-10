#!/usr/bin/env bash
set -euo pipefail
NE_ADDON_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$NE_ADDON_ROOT"
mkdir -p build/host
for test_name in addon_policy note_effect_policy cutout_policy; do
  "${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations \
    -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Iinclude -Iextern/includes "tests/${test_name}_test.cpp" -o "build/host/${test_name}_test"
  "build/host/${test_name}_test"
done
python3 scripts/check_contract.py
