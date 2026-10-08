"""Compile the current native grab/drop and quest code, never a copied model."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
groups = {
    'src/game/interaction.c': ['mario_grab_used_object', 'mario_drop_held_object'],
    'src/game/object_helpers.c': [
        'obj_set_held_state', 'dist_between_objects', 'cur_obj_enable_rendering',
        'cur_obj_disable_rendering', 'cur_obj_become_tangible', 'cur_obj_become_intangible',
        'cur_obj_unrender_and_reset_state', 'cur_obj_move_after_thrown_or_dropped',
        'cur_obj_get_dropped'],
    'src/game/behaviors/tuxie.inc.c': [
        'tuxies_mother_act_0_continue_dialog', 'tuxies_mother_act_0',
        'tuxies_mother_act_1_continue_dialog', 'tuxies_mother_act_1',
        'bhv_small_penguin_loop'],
}
out = ['/* Exact production function bodies extracted for isolated native integration. */']
for filename, names in groups.items():
    source = (root / filename).read_text()
    for name in names:
        match = re.search(r'^\w[\w *]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
        if not match:
            raise RuntimeError(name)
        end = source.index('{', match.start()) + 1
        depth = 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        out.append(source[match.start():end])
Path(sys.argv[1]).write_text('\n\n'.join(out) + '\n')
