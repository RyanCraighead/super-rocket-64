#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-contacts-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
python3 codex/rocketleague/tests/extract_contact_allocation.py "$OUT/contacts_allocation.inc"
python3 -B - "$OUT/network_lookup.c" <<'PYLOOKUP'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
code=Path('src/pc/network/network_player.c').read_text()
Path(sys.argv[1]).write_text('#include "pc/network/network.h"\nstruct NetworkPlayer gNetworkPlayers[MAX_PLAYERS];\nstruct NetworkPlayer *gNetworkPlayerLocal;\n'+function(code,'network_player_from_global_index')+function(code,'get_network_player_smallest_global'))
PYLOOKUP
"${CC:-gcc}" "${FLAGS[@]}" -Wall -Wextra -Werror -Wno-unused-variable \
    -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" \
    codex/rocketleague/tests/test_contacts.c src/pc/character_net.c src/pc/character_net_codec.c \
    "$OUT/network_lookup.c" src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/contacts"
"$OUT/contacts"
