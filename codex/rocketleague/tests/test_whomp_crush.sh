#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-crush.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 -B - "$OUT/native_squish.inc.c" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
s=Path('src/game/mario.c').read_text()
table=s[s.index('u8 sSquishScaleOverTime'):];table=table[:table.index(';')+1]
Path(sys.argv[1]).write_text(table+'\n'+function(s,'squish_mario_model')+'\n'+function(Path('src/game/mario_actions_cutscene.c').read_text(),'act_squished'))
PY
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG -DTEST_NATIVE_MATH \
 -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_whomp_crush.c \
 src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/native"
"$OUT/native"
