#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-quicksand-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 - "$OUT/quicksand-native.inc.c" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
Path(sys.argv[1]).write_text(function(Path('src/game/mario_step.c').read_text(),'mario_update_quicksand')+'\n'+function(Path('src/game/mario_actions_moving.c').read_text(),'quicksand_jump_land_action')+'\n'+function(Path('src/game/mario_actions_cutscene.c').read_text(),'act_quicksand_death'))
Path(sys.argv[1]).with_name('quicksand-update.inc.c').write_text(function(Path('src/game/mario_step.c').read_text(),'mario_update_quicksand'))
PY
gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -ffunction-sections -fdata-sections -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US \
  -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" \
  codex/rocketleague/tests/test_quicksand.c src/pc/rocket_bindings.c -Wl,--gc-sections -lm -o "$OUT/quicksand"
"$OUT/quicksand"
gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -ffunction-sections -fdata-sections -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US \
  -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" \
  codex/rocketleague/tests/test_quicksand_adapter.c src/game/rocket_quicksand.c \
  -Wl,--gc-sections -lm -o "$OUT/adapter"
"$OUT/adapter"
