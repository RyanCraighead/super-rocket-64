#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-water-health.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=undefined -fno-sanitize-recover=all); fi
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_water_health.c \
    -Wl,--gc-sections -lm -o "$OUT/health"
"$OUT/health"
