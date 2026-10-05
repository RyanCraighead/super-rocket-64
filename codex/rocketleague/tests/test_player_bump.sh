#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
out="$(mktemp -d "${TMPDIR:-/tmp}/player-bump.XXXXXX")"
trap 'rm -rf "$out"' EXIT
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG -I. -Iinclude -Isrc -Ilib/lua/include \
 codex/rocketleague/tests/test_player_bump.c src/engine/math_util.c src/pc/network/packets/packet_read_write.c \
 -Wl,--gc-sections -lm -o "$out/bump"
"$out/bump"

cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Werror=implicit-function-declaration \
 -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG -I. -Iinclude -Isrc -Ilib/lua/include \
 codex/rocketleague/tests/test_player_bump_ingress.c src/pc/character_net.c src/pc/character_net_codec.c \
 src/engine/math_util.c src/pc/network/packets/packet_read_write.c -Wl,--gc-sections -lm -lz -o "$out/ingress"
"$out/ingress"
