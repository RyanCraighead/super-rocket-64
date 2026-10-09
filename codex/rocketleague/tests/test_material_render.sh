#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="${MATERIAL_TEST_OUT:-$(mktemp -d "${TMPDIR:-/tmp}/rocket-material-test.XXXXXX")}"
if [[ -z "${MATERIAL_TEST_OUT:-}" ]]; then trap 'rm -rf "$OUT"' EXIT; fi
mkdir -p "$OUT"
g++ -std=c++17 -O1 -g -ffunction-sections -fdata-sections -DROCKET_CAR -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US \
    -I. -Iinclude -Isrc -Ilib/lua/include $(pkg-config --cflags sdl2) codex/rocketleague/tests/test_material_render.cpp \
    -Wl,--gc-sections $(pkg-config --libs sdl2) -lGL -o "$OUT/material-render"
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 MESA_GL_VERSION_OVERRIDE=3.0 MESA_GLSL_VERSION_OVERRIDE=130 "$OUT/material-render" gl "$@"
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 MESA_GL_VERSION_OVERRIDE=2.1 MESA_GLSL_VERSION_OVERRIDE=120 "$OUT/material-render" legacy "$@"
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 MESA_GLES_VERSION_OVERRIDE=2.0 "$OUT/material-render" es "$@"
