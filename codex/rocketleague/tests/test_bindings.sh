#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-bindings-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -Wall -Wextra -Werror)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" codex/rocketleague/tests/test_bindings.c src/pc/rocket_bindings.c -lm -o "$OUT/bindings"
"$OUT/bindings"
"${CC:-cc}" "${FLAGS[@]}" codex/rocketleague/tests/test_gamepad.c -lm -o "$OUT/gamepad"
"$OUT/gamepad"
"${CC:-cc}" "${FLAGS[@]}" -Wno-unused-variable -D_LANGUAGE_C -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -ffunction-sections -fdata-sections -I. -Iinclude -Isrc -Ilib/lua/include \
    $(sdl2-config --cflags) codex/rocketleague/tests/test_bindings_sdl.c src/pc/rocket_bindings.c \
    -Wl,--gc-sections $(sdl2-config --libs) -lm -o "$OUT/sdl"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$OUT/sdl"
