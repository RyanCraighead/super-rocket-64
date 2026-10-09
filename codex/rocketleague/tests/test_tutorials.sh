#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)";cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/tutorials.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
mkdir -p .build/tutorial-tests
python3 - "$OUT/tutorial_native.inc.c" "$OUT/tutorial_widths.inc.c" <<'PY'
from pathlib import Path
import re,sys
sys.path.insert(0,'codex/rocketleague/tests')
from native_slice import function
s=Path('src/game/ingame_menu.c').read_text()
width=s[s.index('u8 gDialogCharWidths[256]'):];width=width[:width.index('};')+2]
Path(sys.argv[2]).write_text(width+'\n')
for name,guard in [('handle_dialog_scroll_page_state','VERSION_EU'),('handle_dialog_text_and_pages','VERSION_JP')]:
    pattern=r'#if(?:def '+guard+r'| defined\('+guard+r'\))\nvoid '+name+r'[^\{]+?#else\n([^#]+)#endif'
    s,count=re.subn(pattern,lambda m:m[1].rstrip(),s,count=1)
    assert count==1,name
names=['handle_dialog_hook','create_dialog_box','create_dialog_inverted_box','handle_dialog_scroll_page_state','handle_dialog_text_and_pages','render_dialog_entries']
Path(sys.argv[1]).write_text('\n'.join(function(s,n) for n in names))
PY
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-error=format-truncation -Wno-unused-variable -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -D_LANGUAGE_C -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
 -I. -Iinclude -Isrc -Isrc/pc/controller -Ilib/lua/include -I"$OUT" $(sdl2-config --cflags) \
 codex/rocketleague/tests/test_tutorials.c src/pc/rocket_bindings.c src/pc/controller/controller_bind_mapping.c src/game/level_info.c \
 -Wl,--gc-sections $(sdl2-config --libs) -lm -o "$OUT/tutorials"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$OUT/tutorials" .build/tutorial-tests
