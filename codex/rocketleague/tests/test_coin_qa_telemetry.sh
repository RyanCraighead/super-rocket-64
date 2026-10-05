#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/coin-qa-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all)
INCLUDES=(-DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG -I. -Iinclude -Isrc -Ilib/lua/include)
gcc "${FLAGS[@]}" "${INCLUDES[@]}" -fno-sanitize=address -Wno-error=implicit-fallthrough -Wno-error=unused-variable \
 -ffunction-sections -fdata-sections -c src/game/interaction.c -o "$OUT/interaction.o"
gcc "${FLAGS[@]}" "${INCLUDES[@]}" -DROCKET_CAR_QA codex/rocketleague/tests/test_coin_qa_telemetry.c \
 src/pc/network/packets/packet_read_write.c "$OUT/interaction.o" -Wl,--gc-sections -lm -o "$OUT/coin-qa"
"$OUT/coin-qa"
gcc "${FLAGS[@]}" "${INCLUDES[@]}" -DROCKET_CAR_QA codex/rocketleague/tests/test_coin_followup_driver.c \
 src/pc/network/packets/packet_read_write.c "$OUT/interaction.o" -Wl,--gc-sections -lm -o "$OUT/coin-driver"
"$OUT/coin-driver"
