"""Extract unchanged production function bodies for the inert native cap fixture.

This avoids linking the complete game/audio/Lua runtime. It is source-level
native behavior coverage, not a ROM playthrough or a save-file persistence test.
"""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[3]
targets = {
    'src/game/behaviors/capswitch.inc.c': ['cap_switch_act_1'],
    'src/game/behaviors/exclamation_box.inc.c': ['exclamation_box_act_0', 'exclamation_box_act_1', 'exclamation_box_act_2'],
    'src/game/interaction.c': ['get_mario_cap_flag', 'interact_cap'],
    'src/game/mario.c': ['update_and_return_cap_flags'],
}
behaviors, interactions = [], []
for path, names in targets.items():
    result = behaviors if '/behaviors/' in path else interactions
    source = (root / path).read_text()
    for name in names:
        match = re.search(r'^\w+\s+' + name + r'\([^\n]*\)\s*\{', source, re.M)
        if not match:
            raise SystemExit(f'Missing native function: {name}')
        depth, end = 1, match.end()
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        result.append(f'#line {source[:match.start()].count(chr(10))+1} "{path}"\n' + source[match.start():end])
source = (root / 'src/game/mario.c').read_text()
interactions.insert(0, re.search(r'^u64 sCapFlickerFrames = [^;]+;', source, re.M).group())
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
(out / 'native_vanish_behaviors.inc').write_text('\n'.join(behaviors) + '\n')
(out / 'native_vanish_interactions.inc').write_text('\n'.join(interactions) + '\n')
