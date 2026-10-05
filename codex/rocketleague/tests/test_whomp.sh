#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-whomp-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" -Wall -Wextra -Werror codex/rocketleague/tests/test_whomp_impact.c -lm -o "$OUT/policy"
"$OUT/policy"
"${CC:-cc}" "${FLAGS[@]}" -Werror=implicit-function-declaration -Wno-incompatible-pointer-types \
    -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_whomp_host.c \
    src/pc/character_net.c src/pc/character_net_codec.c src/pc/network/network_player.c src/engine/math_util.c \
    -Wl,--gc-sections -lm -o "$OUT/native"
"$OUT/native"

"${CC:-cc}" "${FLAGS[@]}" -Werror=implicit-function-declaration -Wno-incompatible-pointer-types \
    -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_switch_host.c \
    src/pc/character_net.c src/pc/character_net_codec.c src/pc/network/network_player.c src/engine/math_util.c \
    -Wl,--gc-sections -lm -o "$OUT/switch"
"$OUT/switch"
