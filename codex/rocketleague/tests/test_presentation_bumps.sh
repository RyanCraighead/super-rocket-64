#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/presentation-bumps-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fno-fast-math -ffp-contract=off
    -Werror=implicit-function-declaration -Wno-incompatible-pointer-types -I. -Iinclude -Isrc -Ilib/lua/include
    -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" codex/rocketleague/tests/test_bobomb.c src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/bobomb"
"$OUT/bobomb"
"${CC:-cc}" "${FLAGS[@]}" codex/rocketleague/tests/test_presentation.c src/game/rocket_pole.c src/engine/math_util.c src/pc/character_net_codec.c -Wl,--gc-sections -lm -o "$OUT/presentation"
"$OUT/presentation"
# Exercise the actual public packet path; the old QA-log verifier was private.
bash codex/rocketleague/tests/test_network.sh
