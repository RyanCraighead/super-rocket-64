#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/text-interaction-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
extra=()
native=src/game/interaction.c
if [[ ${1:-} == --baseline ]]; then
    git show "${2:?baseline commit required}:src/game/interaction.c" > "$OUT/interaction.c"
    git show "$2:src/game/rocket_adapter.c" > "$OUT/rocket_adapter.c"
    native="$OUT/interaction.c"
    extra+=("-DROCKET_ADAPTER_SOURCE=\"$OUT/rocket_adapter.c\"")
fi
python3 - "$OUT/native_text_functions.inc.c" "$native" <<'PY'
from pathlib import Path
import sys
source=Path(sys.argv[2]).read_text()
signatures=['u32 object_facing_mario(', 's16 mario_obj_angle_to_object(', 'u32 mario_can_talk(', 'u32 check_read_sign(', 'u32 check_npc_talk(', 'u32 interact_text(']
result=['#define READ_MASK (INPUT_B_PRESSED | INPUT_A_PRESSED)\n#define SIGN_RANGE 0x4000']
for sig in signatures:
    start=source.index(sig);opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    result.append(source[start:end])
Path(sys.argv[1]).write_text('\n'.join(result)+'\n')
PY
FLAGS=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -ffunction-sections -fdata-sections
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -I. -Iinclude -Isrc -Isrc/game -Ilib/lua/include -I"$OUT")
if [[ ${SANITIZE:-1} == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" "${extra[@]}" codex/rocketleague/tests/test_text_interaction.c src/engine/math_util.c -Wl,--gc-sections -lm -o "$OUT/text"
"$OUT/text"
