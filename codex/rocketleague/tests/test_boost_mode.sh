#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-boost-mode-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_boost_mode.c \
    src/pc/rocket_boost.c src/pc/network/version.c src/pc/character_net_codec.c src/pc/character_net.c \
    src/pc/network/packets/packet_read_write.c -Wl,--gc-sections -lm -lz -o "$OUT/rule"
"$OUT/rule"
"${CC:-gcc}" "${FLAGS[@]}" -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_boost_config.c src/pc/rocket_bindings.c \
    -Wl,--gc-sections -lm -o "$OUT/config"
"$OUT/config" "$OUT"
# Compile the actual settings UI and join/init integration (no SDL window).
"${CC:-gcc}" -std=gnu11 -fsyntax-only -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US \
    -I. -Iinclude -Isrc -Ilib/lua/include src/pc/djui/djui_rocket_boost.c \
    src/pc/djui/djui_panel_options.c src/pc/network/packets/packet_join.c src/pc/network/network.c
