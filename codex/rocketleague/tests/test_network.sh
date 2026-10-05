#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/character-net-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror codex/rocketleague/tests/test_character_net.c \
    src/pc/character_net_codec.c -lm -o "$OUT/codec"
"$OUT/codec"
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_character_transport.c \
    src/pc/character_net_codec.c src/pc/character_net.c src/pc/network/packets/packet_read_write.c \
    -Wl,--gc-sections -lm -lz -o "$OUT/transport"
"$OUT/transport"
