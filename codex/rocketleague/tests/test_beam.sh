#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-beam-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 - "$OUT/beam-native.inc.c" <<'PY'
from pathlib import Path
import re,sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
level=Path('src/game/level_update.c').read_text()
script=re.sub(r'/\*.*?\*/','',Path('levels/castle_inside/script.c').read_text(),flags=re.S)
warp=re.search(r'(?m)^\s*WARP_NODE\(\s*0xF2\s*,([^)]*)\)',script)
assert warp
values=[x.strip() for x in warp[1].split(',')]
assert values==['LEVEL_TOTWC','0x01','0x0A','WARP_NO_CHECKPOINT']
code='static const struct WarpNode nativeBeamNode={0xF2,'+','.join(values[:3])+'};\n'
code+=function(Path('src/game/area.c').read_text(),'area_get_warp_node')
code+='\n'.join(function(level,n) for n in ['verify_warp','level_trigger_warp','initiate_delayed_warp'])
Path(sys.argv[1]).write_text(code)
PY
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Werror=implicit-function-declaration \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -DDISABLE_MODULE_LOG \
 -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_beam.c \
 src/game/rocket_beam.c -Wl,--gc-sections -lm -o "$OUT/beam"
"$OUT/beam"
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Werror=implicit-function-declaration \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
 -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_beam_adapter.c \
 src/game/rocket_beam.c src/pc/rocket_bindings.c -Wl,--gc-sections -lm -o "$OUT/adapter"
"$OUT/adapter"
