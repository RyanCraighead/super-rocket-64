#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-enemy-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
python3 -B - "$OUT/network_lookup.c" <<'PYLOOKUP'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
code=Path('src/pc/network/network_player.c').read_text()
Path(sys.argv[1]).write_text('#include "pc/network/network.h"\nstruct NetworkPlayer gNetworkPlayers[MAX_PLAYERS];\nstruct NetworkPlayer *gNetworkPlayerLocal;\n'+function(code,'network_player_from_global_index')+function(code,'get_network_player_smallest_global'))
PYLOOKUP
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror codex/rocketleague/tests/test_enemy_impact.c -lm -o "$OUT/policy"
"$OUT/policy"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include -c src/game/rocket_enemy.c -o "$OUT/enemy.o"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -Wno-unused-variable -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_enemy_host.c \
    src/pc/character_net.c src/pc/character_net_codec.c "$OUT/network_lookup.c" -Wl,--gc-sections -lm -o "$OUT/host"
"$OUT/host"
python3 codex/rocketleague/tests/extract_enemy_native.py "$OUT/enemy_native_bodies.inc"
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_enemy_native.c -lm -o "$OUT/native"
"$OUT/native"
VENDOR=codex/rocketleague/vendor/RocketSim
python3 -B codex/rocketleague/tools/verify_vendor.py "$VENDOR" codex/rocketleague/vendor/RocketSim.SHA256.json
"${CXX:-g++}" -std=c++20 -O1 -fno-fast-math -ffp-contract=off -I"$VENDOR/src" -I"$VENDOR/libsrc/bullet3-3.24" \
    codex/rocketleague/tests/test_enemy_vendor.cpp "$VENDOR/src/Sim/Car/CarConfig/CarConfig.cpp" -o "$OUT/vendor"
"$OUT/vendor"
