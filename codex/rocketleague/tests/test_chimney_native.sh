#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-chimney-native.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
python3 -B codex/rocketleague/tests/extract_chimney_native.py "$OUT/chimney_native.inc"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -Wno-unused-variable -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_chimney_native.c \
    -Wl,--gc-sections -lm -o "$OUT/chimney"
"$OUT/chimney"
