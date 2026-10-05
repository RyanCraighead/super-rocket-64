#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-tree.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
BASELINE="${1:-}"
python3 -B - "$OUT" "$BASELINE" <<'PY'
from pathlib import Path
import subprocess,sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
tail=function(Path('src/game/object_collision.c').read_text(),'detect_object_hitbox_overlap')+function(Path('src/game/mario_actions_automatic.c').read_text(),'set_pole_position')
sources=[('current',Path('src/game/interaction.c').read_text())]
if sys.argv[2]: sources.insert(0,('baseline',subprocess.check_output(['git','show',sys.argv[2]+':src/game/interaction.c'],text=True)))
for name,text in sources:
    p=Path(sys.argv[1])/name;p.mkdir()
    (p/'native_tree.inc.h').write_text(function(text,'interact_pole')+tail)
PY
kinds=(current);if [[ -n $BASELINE ]];then kinds=(baseline current);fi
for kind in "${kinds[@]}"; do
 extra=();if [[ $kind == baseline ]];then extra+=(-DTREE_BASELINE -Wno-unused-function);fi
 cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
  -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror \
  -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG -DTEST_NATIVE_MATH \
  -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT/$kind" "${extra[@]}" codex/rocketleague/tests/test_tree_contact.c \
  src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/$kind/native"
 "$OUT/$kind/native"
done
