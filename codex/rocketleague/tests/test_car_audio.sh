#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-audio-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
: "${ROCKET_PHYSICS_BUILD:?Set ROCKET_PHYSICS_BUILD to the native CMake build}"
g++ -std=c++20 -O1 -g -fno-fast-math -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -pthread \
 codex/rocketleague/tests/test_car_audio.cpp "$ROCKET_PHYSICS_BUILD/librocket_physics.a" \
 "$ROCKET_PHYSICS_BUILD/RocketSim/libRocketSim.a" -o "$OUT/audio"
"$OUT/audio" "$@"
gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
 -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -I. -Iinclude -Isrc -Ilib/lua/include \
 codex/rocketleague/tests/test_car_native_sounds.c -o "$OUT/native"
"$OUT/native"
python3 -B codex/rocketleague/tests/test_car_audio_setup.py
