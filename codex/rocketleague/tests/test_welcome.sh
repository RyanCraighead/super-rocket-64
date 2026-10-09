#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../../.."
OUT="${WELCOME_TEST_OUT:-.build/welcome-tests}"
mkdir -p "$OUT"
python3 codex/rocketleague/tests/extract_welcome_native.py "$OUT"
cc -std=gnu11 -O1 -g -fwrapv -ffunction-sections -fdata-sections -fsanitize=undefined -fno-sanitize-recover=all \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" \
 $(sdl2-config --cflags) codex/rocketleague/tests/test_welcome.c src/game/rocket_welcome.c src/engine/math_util.c \
 -Wl,--gc-sections -lm -o "$OUT/welcome"
"$OUT/welcome"
