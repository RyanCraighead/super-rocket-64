#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-metal-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 codex/rocketleague/tests/extract_metal_native.py "$OUT"
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=undefined -fno-sanitize-recover=all); fi
for fixture in health native progression water; do
    "${CC:-cc}" "${FLAGS[@]}" -Werror=implicit-function-declaration -Wno-incompatible-pointer-types \
        -I. -Iinclude -Isrc -Isrc/game -Ilib/lua/include -I"$OUT" \
        -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DROCKET_NATIVE_SLICE \
        "codex/rocketleague/tests/test_metal_$fixture.c" -Wl,--gc-sections -lm -o "$OUT/$fixture"
    "$OUT/$fixture"
done
