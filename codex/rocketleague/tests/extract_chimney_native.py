"""Copy exact native warp/dispatch bodies and CCM script metadata for a fixture.

No production logic is replaced. Unused interaction handlers are listed for
explicit aborting boundary stubs; the native handler table itself is verbatim.
"""
from pathlib import Path
import re
import sys
from native_slice import function

ROOT = Path(__file__).resolve().parents[3]


def extract(output):
    interaction = (ROOT / 'src/game/interaction.c').read_text()
    definition = re.search(r'^struct InteractionHandler \{.*?^};', interaction, re.M | re.S)
    table = re.search(r'^static struct InteractionHandler sInteractionHandlers\[\] = \{.*?^};', interaction, re.M | re.S)
    if not definition or not table:
        raise ValueError('Native interaction table missing')
    handlers = sorted(set(re.findall(r'\{\s*INTERACT_\w+\s*,\s*(\w+)\s*}', table[0])))
    assert 'interact_warp' in handlers
    result = ['/* Generated from current native source; do not edit. */\n', definition[0], table[0]]
    # The boundary macro aborts if this warp-only fixture calls another handler.
    result += ['CHIMNEY_UNUSED_INTERACTION('+name+')' for name in handlers if name != 'interact_warp']
    for name in ('sDelayInvincTimer', 'gInteractionInvulnerable', 'sDisplayingDoorText', 'sJustTeleported'):
        match = re.search(r'^(?:static )?(?:u8|s16) '+name+r'\b[^;]*;', interaction, re.M)
        if not match:
            raise ValueError('Native global missing: '+name)
        result.append(match[0])
    sources = {
        'src/game/interaction.c': ('mario_get_collided_object', 'mario_stop_riding_object', 'interact_warp', 'mario_process_interactions'),
        'src/game/mario_step.c': ('stop_and_set_height_to_floor',),
        'src/game/behaviors/warp.inc.c': ('bhv_warp_loop',),
        'src/game/mario_actions_cutscene.c': ('act_disappeared',),
        'src/game/area.c': ('area_get_warp_node', 'area_get_warp_node_from_params'),
    }
    for path, names in sources.items():
        text = (ROOT/path).read_text()
        for name in names:
            body = function(text, name)
            line = text[:text.index(body.rstrip())].count('\n')+1
            result.append(f'#line {line} "{path}"\n'+body)
    script = re.sub(r'/\*.*?\*/|//[^\n]*', '', (ROOT/'levels/ccm/script.c').read_text(), flags=re.S)
    area = re.search(r'\bAREA\(\s*1\s*,.*?END_AREA\(\)', script, re.S)
    if not area:
        raise ValueError('Native CCM area 1 missing')
    objects = re.findall(r'\bOBJECT\(([^\n]*?\bbhvWarp)\s*\)', area[0])
    if len(objects) != 1:
        raise ValueError('Expected one native nonfading CCM warp')
    params = objects[0].split(',')[-2].strip()
    node_id = (int(params, 0) >> 16) & 255
    nodes = [m for m in re.findall(r'\bWARP_NODE\(([^)]*)\)', area[0])
             if int(m.split(',')[0].strip(), 0) == node_id]
    if len(nodes) != 1:
        raise ValueError('Canonical CCM chimney warp node missing')
    values = [p.strip() for p in nodes[0].split(',')]
    result.append(f'#define NATIVE_CCM_CHIMNEY_PARAMS {params}\n')
    result.append('static const struct WarpNode nativeCcmChimneyNode = {'+', '.join(values[:4])+'};\n')
    # Retain the separate native door/slide exit route, not a chimney redirect.
    for index, label in ((1, 'nativeCcmDoorEntryNode'), (2, 'nativeCcmSlideExitNode')):
        part = re.search(r'\bAREA\(\s*'+str(index)+r'\s*,.*?END_AREA\(\)', script, re.S)
        exits = [m for m in re.findall(r'\bWARP_NODE\(([^)]*)\)', part[0])
                 if int(m.split(',')[0].strip(), 0) == 0x14]
        if len(exits) != 1:
            raise ValueError('CCM native door/slide exit node missing')
        fields = [p.strip() for p in exits[0].split(',')]
        result.append('static const struct WarpNode '+label+' = {'+', '.join(fields[:4])+'};\n')
        if index == 2:
            spawns = re.findall(r'\bOBJECT\(([^\n]*?\bbhvAirborneWarp)\s*\)', part[0])
            if len(spawns) != 1:
                raise ValueError('CCM slide airborne destination missing')
            result.append('#define NATIVE_CCM_SLIDE_SPAWN_PARAMS '+spawns[0].split(',')[-2].strip()+'\n')
    Path(output).write_text('\n'.join(result))


if __name__ == '__main__':
    extract(sys.argv[1])
