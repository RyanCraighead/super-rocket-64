#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/coin-boost-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -Wall -Wextra -Werror)
python3 codex/rocketleague/tests/extract_coin_lifecycle_native.py "$OUT/coin_lifecycle_native.inc"
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
# ASan global registration retains the unrelated interaction-handler table and
# its entire game graph. Keep UBSan on this TU and ASan+UBSan on the packet test.
"${CC:-gcc}" "${FLAGS[@]}" -fno-sanitize=address -Wno-error=implicit-fallthrough -Wno-error=unused-variable -ffunction-sections -fdata-sections \
    -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -c src/game/interaction.c -o "$OUT/interaction.o"
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -DCOIN_NATIVE_LIFECYCLE -I"$OUT" -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_coin_boost.c \
    src/pc/network/packets/packet_read_write.c "$OUT/interaction.o" -Wl,--gc-sections -lm -o "$OUT/coin-boost"
"$OUT/coin-boost"
