#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-boss-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fno-fast-math -ffp-contract=off \
    codex/rocketleague/tests/test_boss_impact.c -lm -o "$OUT/contact"
"$OUT/contact"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Isrc \
    codex/rocketleague/tests/test_boss_net_protocol.c src/pc/boss_net_protocol.c -o "$OUT/protocol"
"$OUT/protocol"
HOST_FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then HOST_FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${HOST_FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_boss_net_host.c \
    src/pc/boss_net_protocol.c src/pc/network/packets/packet_read_write.c \
    src/game/obj_behaviors.c src/game/object_helpers.c \
    -Wl,--gc-sections -lm -o "$OUT/network-host"
"$OUT/network-host"
"${CC:-cc}" -std=gnu11 -O1 -ffunction-sections -fdata-sections -fno-fast-math -ffp-contract=off \
    -Wno-incompatible-pointer-types -Werror=implicit-function-declaration -I. -Iinclude -Isrc -Ilib/lua/include \
    -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    codex/rocketleague/tests/test_boss_native.c src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/native"
"$OUT/native"
python3 codex/rocketleague/tests/test_boss_acceptance.py
python3 codex/rocketleague/tests/test_boss_network_acceptance.py
python3 codex/rocketleague/tests/test_mario_boss_acceptance.py
