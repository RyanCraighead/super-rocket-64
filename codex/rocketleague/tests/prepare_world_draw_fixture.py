"""Copy unmodified production dispatch functions into a windowless fixture."""
from pathlib import Path
import re
import sys
from native_slice import function

root = Path(__file__).resolve().parents[3]
sources = {
    'src/pc/gfx/gfx_pc.c': ('gfx_codex_set_world_draw', 'gfx_codex_world_frame_begin',
                          'gfx_codex_get_camera', 'gfx_codex_world_noop'),
    'src/pc/gfx/gfx_sdl.c': ('gfx_sdl_draw_rocket_world', 'gfx_sdl_draw_other_presentation'),
}
parts = []
for path, names in sources.items():
    text = (root/path).read_text()
    parts.extend(function(text, name) for name in names)
pc = (root/'src/pc/gfx/gfx_pc.c').read_text()
dispatch = re.search(r'case G_NOOP:\s*(.*?)\s*break;', function(pc, 'gfx_run_dl'), re.S)
if not dispatch:
    raise ValueError('Native interpreter has no NOOP dispatch')
parts.append('static void test_native_noop_dispatch(Gfx *cmd) { switch(cmd->words.w0 >> 24) {\n'
             'case G_NOOP: '+dispatch.group(1)+' break; default: break; } }\n')
builder = function((root/'src/game/area.c').read_text(), 'render_game')
parts.append(builder)
parts.append('#define TARGET_N64\n#define render_game render_game_n64\n'+builder+
             '#undef render_game\n#undef TARGET_N64\n')
Path(sys.argv[1]).write_text(''.join(parts))
