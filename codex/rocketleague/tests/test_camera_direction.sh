#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)";cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/camera-direction.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
extra=()
if [[ ${1:-} == --baseline ]]; then
 git show "${2:?baseline required}:src/pc/controller/controller_sdl.c" > "$OUT/controller_sdl.c"
 extra+=("-DCONTROLLER_SDL_SOURCE=\"$OUT/controller_sdl.c\"")
fi
python3 - "$OUT/native_camera_functions.inc.c" <<'PY'
from pathlib import Path
import re,sys
s=Path('src/game/bettercamera.inc.h').read_text();result=[]
for name in ['newcam_clamp','newcam_lengthdir_x','newcam_lengthdir_y','newcam_adjust_value','newcam_ivrt','newcam_rotate_button','newcam_update_values','newcam_position_cam']:
    start=s.rfind('\n',0,s.index(name+'('))+1;opening=s.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}');end+=1
    result.append(s[start:end])
c=Path('src/pc/configfile.c').read_text()
for axis in 'XY':
    result.append('#define DEFAULT_CAMERA_'+axis+' '+re.search(r'configCameraInvert'+axis+r'\s*=\s*(true|false)',c)[1])
Path(sys.argv[1]).write_text('\n'.join(result)+'\n')
PY
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-variable -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -D_LANGUAGE_C -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
 -I. -Iinclude -Isrc -Isrc/pc/controller -Ilib/lua/include -I"$OUT" "${extra[@]}" $(sdl2-config --cflags) \
 codex/rocketleague/tests/test_camera_direction.c src/pc/rocket_bindings.c src/engine/math_util.c \
 -Wl,--gc-sections $(sdl2-config --libs) -lm -o "$OUT/camera"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$OUT/camera"
