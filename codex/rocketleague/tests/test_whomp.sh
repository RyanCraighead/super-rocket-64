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
# Only the actual player lookup is needed by these headless tests. Avoid
# importing the Windows-only transport setup into the Linux native fixture.
python3 -B - "$OUT/network_lookup.c" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
code=Path('src/pc/network/network_player.c').read_text()
Path(sys.argv[1]).write_text('#include "pc/network/network.h"\nstruct NetworkPlayer gNetworkPlayers[MAX_PLAYERS];\nstruct NetworkPlayer *gNetworkPlayerLocal;\n'+function(code,'network_player_from_global_index'))
PY
"${CC:-cc}" "${FLAGS[@]}" -Werror=implicit-function-declaration -Wno-incompatible-pointer-types \
    -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_whomp_host.c \
    src/pc/character_net.c src/pc/character_net_codec.c "$OUT/network_lookup.c" src/engine/math_util.c \
    -Wl,--gc-sections -lm -o "$OUT/native"
"$OUT/native"

"${CC:-cc}" "${FLAGS[@]}" -Werror=implicit-function-declaration -Wno-incompatible-pointer-types \
    -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_switch_host.c \
    src/pc/character_net.c src/pc/character_net_codec.c "$OUT/network_lookup.c" src/engine/math_util.c \
    -Wl,--gc-sections -lm -o "$OUT/switch"
"$OUT/switch"
