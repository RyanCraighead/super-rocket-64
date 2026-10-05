#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-enemy-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror codex/rocketleague/tests/test_enemy_impact.c -lm -o "$OUT/policy"
"$OUT/policy"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include -c src/game/rocket_enemy.c -o "$OUT/enemy.o"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -Wno-unused-variable -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_enemy_host.c \
    src/pc/character_net.c src/pc/character_net_codec.c src/pc/network/network_player.c -Wl,--gc-sections -lm -o "$OUT/host"
"$OUT/host"
python3 codex/rocketleague/tests/extract_enemy_native.py "$OUT/enemy_native_bodies.inc"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_enemy_native.c -lm -o "$OUT/native"
"$OUT/native"
VENDOR=codex/rocketleague/vendor/RocketSim
test "$(git -C "$VENDOR" rev-parse HEAD)" = c2baacb8f4b441dd8505e63c2aeb5a1679b60b02
"${CXX:-g++}" -std=c++20 -O1 -fno-fast-math -ffp-contract=off -I"$VENDOR/src" -I"$VENDOR/libsrc/bullet3-3.24" \
    codex/rocketleague/tests/test_enemy_vendor.cpp "$VENDOR/src/Sim/Car/CarConfig/CarConfig.cpp" -o "$OUT/vendor"
"$OUT/vendor"
