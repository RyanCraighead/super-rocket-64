#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)";cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-hud-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
source=Path('src/game/hud.c').read_text();out=Path(sys.argv[1])
(out/'hud-native-headers.inc.h').write_text(source[:source.index('extern bool gDjuiInMainMenu;')])
(out/'hud-native.inc.c').write_text('\n'.join(function(source,name) for name in ['rocket_boost_hud_snapshot','render_hud_camera_status','boost_meter_rect','render_rocket_boost_hud']))
PY
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-variable -ffunction-sections -fdata-sections \
 -fsanitize=undefined -fno-sanitize-recover=all -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
 -I. -Iinclude -Isrc -Isrc/game -Ilib/lua/include -I"$OUT" \
 codex/rocketleague/tests/test_boost_hud.c src/pc/rocket_bindings.c -Wl,--gc-sections -lm -o "$OUT/hud"
mkdir -p .build/hud-inspection
"$OUT/hud" .build/hud-inspection
