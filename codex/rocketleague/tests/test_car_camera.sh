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
python3 -B - "$OUT/car_camera_native.inc.h" <<'PY'
from pathlib import Path
import sys,re
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
s=Path('src/game/bettercamera.inc.h').read_text()
update=function(Path('src/game/camera.c').read_text(),'update_camera')
start=update.index('rocket_camera_input_begin(c)')
end=update.index('rocket_camera_input_end(carInput)')
assert start<update.index('if (c->cutscene == 0)')<end
assert 'return;' not in update[start:end], 'Camera input must be restored on every exit'
constants=re.search(r'static const f32 NEWCAM_DISTANCES\[\] = \{.*?;',s,re.S)[0]
constants+=re.search(r'static const u32 NEWCAM_NUM_DISTANCES = .*?;',s)[0]
constants+='\n'+re.search(r'#define NEWCAM_DISTANCE_INC[^\n]+',s)[0]+'\n'
Path(sys.argv[1]).write_text(constants+function(s,'newcam_get_distance_target')+function(s,'newcam_zoom_button')+function(s,'newcam_stick_input')+function(s,'newcam_collision'))
PY
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-variable -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -D_LANGUAGE_C -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
 -I. -Iinclude -Isrc -Isrc/pc/controller -Ilib/lua/include -I"$OUT" "${extra[@]}" $(sdl2-config --cflags) \
 codex/rocketleague/tests/test_car_camera.c src/pc/rocket_bindings.c src/engine/math_util.c \
 -Wl,--gc-sections $(sdl2-config --libs) -lm -o "$OUT/camera"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$OUT/camera"
