#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-lava-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 codex/rocketleague/tests/extract_lava_native.py "$OUT/lava-native.inc.c" "$OUT/lava-input.inc.cpp"
FLAGS=(-O1 -g -fno-fast-math -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections)
INCLUDES=(-I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB)
"${CC:-cc}" -std=gnu11 "${FLAGS[@]}" -Werror=implicit-function-declaration "${INCLUDES[@]}" \
    codex/rocketleague/tests/test_lava.c src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/lava"
"$OUT/lava"
"${CC:-cc}" -std=gnu11 "${FLAGS[@]}" "${INCLUDES[@]}" -c src/pc/rocket_bindings.c -o "$OUT/bindings.o"
"${CXX:-c++}" -std=c++17 "${FLAGS[@]}" "${INCLUDES[@]}" codex/rocketleague/tests/test_lava_input.cpp \
    "$OUT/bindings.o" -Wl,--gc-sections -lm -o "$OUT/input"
"$OUT/input"
