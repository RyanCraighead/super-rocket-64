#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-enemy-visibility.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_enemy_visibility.c \
    -Wl,--gc-sections -lm -o "$OUT/visibility"
"$OUT/visibility"
