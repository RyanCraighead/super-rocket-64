#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-pole-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 codex/rocketleague/tests/extract_lava_native.py "$OUT/lava-native.inc.c" "$OUT/lava-input.inc.cpp"
python3 - "$OUT/pole-native.inc.c" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
s=Path('src/game/mario_actions_automatic.c').read_text()
names=['add_tree_leaf_particles','play_climbing_sounds','set_pole_position','act_holding_pole','act_climbing_pole','act_grab_pole_slow','act_grab_pole_fast','act_top_of_pole_transition','act_top_of_pole','check_common_automatic_cancels','mario_execute_automatic_action']
Path(sys.argv[1]).write_text(function(Path('src/game/interaction.c').read_text(),'interact_pole')+'\n'+'\n'.join(function(s,n) for n in names))
PY
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Werror=implicit-function-declaration \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG \
 -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_pole.c \
 src/game/rocket_pole.c src/engine/math_util.c src/pc/rocket_bindings.c -Wl,--gc-sections -lm -o "$OUT/pole"
"$OUT/pole" "$@"
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Werror=implicit-function-declaration \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DTEST_NATIVE_MATH \
 -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_pole_adapter.c \
 src/game/rocket_pole.c src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/adapter"
"$OUT/adapter"
