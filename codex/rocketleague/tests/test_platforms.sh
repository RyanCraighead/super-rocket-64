#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-platform-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -c src/game/obj_behaviors_2.c -o "$OUT/native.o"
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -c src/game/object_helpers.c -o "$OUT/helpers.o"
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -c codex/rocketleague/tests/test_platform_packet.c -o "$OUT/packet.o"
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
python3 - "$OUT/platform_native_lifecycle.inc" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, 'codex/rocketleague/tests')
from native_slice import function
Path(sys.argv[1]).write_text(function(Path('src/game/object_list_processor.c').read_text(),'update_objects')+
    function(Path('src/game/spawn_object.c').read_text(),'allocate_object')+
    function(Path('src/pc/network/network_player.c').read_text(),'get_network_player_smallest_global'))
PY
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_platform_host.c \
    src/pc/character_net.c src/pc/character_net_codec.c src/engine/math_util.c \
    src/pc/network/packets/packet_read_write.c "$OUT/native.o" "$OUT/helpers.o" "$OUT/packet.o" \
    -Wl,--gc-sections -lm -lz -o "$OUT/platforms"
"$OUT/platforms"

python3 - "$OUT/platform_ingress_guards.inc" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, 'codex/rocketleague/tests')
from native_slice import function
boss = Path('src/pc/boss_net.c').read_text()
caps = Path('src/game/rocket_caps.c').read_text()
Path(sys.argv[1]).write_text('#include "game/interaction.h"\n'+function(boss, 'boss_net_legacy_packet_valid')+
    ''.join(function(caps,n) for n in ('cap_flag','online','alive','same_area','get32','contact','pickup_state','rocket_caps_object_allowed'))+
    function(Path('src/pc/network/network_player.c').read_text(),'get_network_player_smallest_global'))
PY
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_platform_ingress.c \
    src/pc/character_net.c src/pc/character_net_codec.c src/engine/math_util.c \
    src/pc/network/packets/packet_read_write.c -Wl,--gc-sections -lm -lz -o "$OUT/ingress"
"$OUT/ingress"
