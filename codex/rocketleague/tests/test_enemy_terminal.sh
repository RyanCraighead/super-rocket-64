#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-enemy-terminal.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
python3 codex/rocketleague/tests/extract_enemy_native.py "$OUT/enemy_native_bodies.inc"
python3 codex/rocketleague/tests/extract_enemy_terminal_native.py "$OUT/enemy_terminal_native.inc"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_enemy_terminal_native.c \
    -Wl,--gc-sections -lm -o "$OUT/terminal"
"$OUT/terminal"
