#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/wing-handoff-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 - "$OUT/native_wing_handoff.inc.c" <<'PY'
from pathlib import Path
import sys
result=[]
for name,signature in [('src/game/level_update.c','void set_mario_initial_cap_powerup('),
                       ('src/game/level_update.c','void set_mario_initial_action('),
                       ('src/game/mario.c','u32 update_and_return_cap_flags(')]:
    source=Path(name).read_text();start=source.index(signature);opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    result.append(source[start:end])
Path(sys.argv[1]).write_text('\n'.join(result)+'\n')
PY
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -Wall -Wextra -Werror
 -ffunction-sections -fdata-sections -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT"
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB)
if [[ ${SANITIZE:-1} == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" codex/rocketleague/tests/test_wing_handoff.c -Wl,--gc-sections -lm -o "$OUT/native"
"$OUT/native"
