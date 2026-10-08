#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT="$PWD/.build/menu-tests"
mkdir -p "$OUT"
python3 - "$OUT" <<'PY'
import re, sys
from pathlib import Path
source = Path('bin/custom_font.c').read_text()
Path(sys.argv[1], 'font-widths.h').write_text(re.search(r'const f32 font_normal_widths\[\] = \{.*?\};', source, re.S)[0])
PY
gcc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" $(sdl2-config --cflags) \
    tests/menu-scroll/test_menu_scroll.c src/pc/rocket_bindings.c src/pc/rocket_boost.c \
    -Wl,--gc-sections $(sdl2-config --libs) -lm -o "$OUT/menu-tests"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$OUT/menu-tests" "$OUT"
gcc -std=gnu11 -O1 -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US \
    -I. -Iinclude -Isrc -Ilib/lua/include tests/menu-scroll/test_scale.c src/pc/djui/djui_gfx.c \
    -Wl,--gc-sections -lm -o "$OUT/scale-test"
"$OUT/scale-test"
echo 'UI scale: 35 resolution/scale combinations passed'
