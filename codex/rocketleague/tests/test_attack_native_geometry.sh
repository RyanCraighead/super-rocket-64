#!/usr/bin/env bash
# Optional owned-asset integration test. Inputs: two little-endian native
# collision streams, blue-switch RocketSnapshot sequence, king rest snapshot.
# Keep generated proprietary collision streams outside the public checkout.
set -euo pipefail
ulimit -c 0
if [[ $# != 4 && $# != 5 ]];then echo 'Usage: test_attack_native_geometry.sh WHOMP_COLLISION BLUE_COLLISION BLUE_POSES KING_POSE [SPEED_PERCENT]' >&2;exit 2;fi
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)";cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-native-attack.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 -B - "$OUT" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
files={'src/game/object_helpers.c':['obj_build_transform_from_pos_and_angle','obj_apply_scale_to_matrix'],
 'src/engine/surface_load.c':['transform_object_vertices','read_surface_data'],
 'src/engine/surface_collision.c':['find_floor_from_list'],
 'src/game/rocket_adapter.c':['solid_surface','native_phase_surface','door_ray_distance','object_visible_along','rocket_adapter_whomp_path_clear'],
 'src/pc/utils/misc.c':['delta_interpolate_f32']}
out=Path(sys.argv[1])
(out/'attack_native_functions.inc.h').write_text('\n'.join(function(Path(name).read_text(),func) for name,funcs in files.items() for func in funcs))
(out/'network_lookup.c').write_text('#include "pc/network/network.h"\nstruct NetworkPlayer gNetworkPlayers[MAX_PLAYERS];\nstruct NetworkPlayer *gNetworkPlayerLocal;\n'+function(Path('src/pc/network/network_player.c').read_text(),'network_player_from_global_index'))
PY
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG \
 -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_attack_native_geometry.c \
 src/pc/character_net.c src/pc/character_net_codec.c "$OUT/network_lookup.c" src/engine/math_util.c \
 -Wl,--gc-sections -lm -o "$OUT/native"
"$OUT/native" "$@"
