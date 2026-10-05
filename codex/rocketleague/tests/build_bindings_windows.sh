#!/usr/bin/env bash
# Source-only native Windows checks. Run resulting EXEs with SDL's dummy drivers.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SDL_ROOT="$(realpath "${1:?Pass the existing MinGW SDL2 prefix}")"
cd "$ROOT"
OUT=codex/rocketleague/.local/bindings-windows
mkdir -p "$OUT"
CC="${CC:-x86_64-w64-mingw32-gcc}"
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off)
"$CC" "${FLAGS[@]}" -Wall -Wextra -Werror codex/rocketleague/tests/test_bindings.c \
    src/pc/rocket_bindings.c -static -lm -o "$OUT/bindings.exe"
HOST_FLAGS=(-D_LANGUAGE_C -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DSDL_MAIN_HANDLED
    -ffunction-sections -fdata-sections -I. -Iinclude -Isrc -Ilib/lua/include -I"$SDL_ROOT/include")
"$CC" "${FLAGS[@]}" "${HOST_FLAGS[@]}" codex/rocketleague/tests/test_bindings_sdl.c \
    src/pc/rocket_bindings.c -Wl,--gc-sections -L"$SDL_ROOT/lib" -lSDL2 -static-libgcc -lm -o "$OUT/sdl.exe"
cp "$SDL_ROOT/bin/SDL2.dll" "$OUT/SDL2.dll"
for source in src/pc/configfile.c src/pc/controller/controller_sdl.c src/pc/controller/controller_keyboard.c \
    src/pc/djui/djui_panel_controls.c src/pc/djui/djui_panel_rocket_controls.c; do
    "$CC" "${FLAGS[@]}" "${HOST_FLAGS[@]}" -c "$source" -o "$OUT/$(basename "$source").o"
done
printf 'PASS MinGW compilation: mapping, real SDL/keyboard fixture, config and both UI panels\n'
