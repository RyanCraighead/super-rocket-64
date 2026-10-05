#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-environment.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 codex/rocketleague/tests/prepare_environment_fixture.py "$OUT/environment_native.inc"
FLAGS=(-std=gnu11 -fgnu89-inline -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -Isrc/game -Isrc/engine -I"$OUT" \
    codex/rocketleague/tests/test_environment_native.c src/game/rocket_environment.c \
    -Wl,--gc-sections -lm -o "$OUT/native"
"$OUT/native"
