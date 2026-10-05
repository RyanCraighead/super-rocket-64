#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-world-draw.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 -B codex/rocketleague/tests/prepare_world_draw_fixture.py "$OUT/world_draw_native.inc"
FLAGS=(-std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fno-fast-math -ffp-contract=off)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" -Wall -Wextra -Werror -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" $(pkg-config --cflags sdl2) \
    codex/rocketleague/tests/test_world_draw.c -Wl,--gc-sections -lm -o "$OUT/world-draw"
"$OUT/world-draw"
