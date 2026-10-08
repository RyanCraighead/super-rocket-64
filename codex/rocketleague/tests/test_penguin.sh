#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-penguin-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 codex/rocketleague/tests/extract_penguin_native.py "$OUT/penguin-native.inc.c"
"${CC:-cc}" -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    codex/rocketleague/tests/test_penguin.c src/pc/rocket_bindings.c -lm -o "$OUT/penguin"
"$OUT/penguin"
"${CC:-cc}" -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    codex/rocketleague/tests/test_penguin_network.c src/game/rocket_penguin.c \
    src/pc/character_net.c src/pc/character_net_codec.c src/pc/network/packets/packet_read_write.c \
    -Wl,--gc-sections -lm -lz -o "$OUT/penguin-network"
"$OUT/penguin-network"
