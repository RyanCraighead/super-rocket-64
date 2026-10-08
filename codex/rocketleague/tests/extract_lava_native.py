"""Production burn entry/action and native air physics in a bounded test world."""
from pathlib import Path
import sys
from native_slice import function
root=Path(__file__).resolve().parents[3]
groups={
    'src/game/mario.c': ['set_mario_y_vel_based_on_fspeed','set_mario_action_airborne',
        'set_mario_action_moving','set_mario_action_submerged','set_mario_action_cutscene','set_mario_action','update_mario_health'],
    'src/game/interaction.c': ['check_lava_boost','mario_handle_special_floors'],
    'src/game/mario_step.c': ['perform_air_quarter_step','apply_twirl_gravity',
        'should_strengthen_gravity_for_jump_ascent','apply_gravity','apply_vertical_wind','perform_air_step'],
    'src/game/mario_actions_airborne.c': ['lava_boost_on_wall','update_lava_boost_or_twirling','act_lava_boost'],
}
out=[]
for filename,names in groups.items():
    source=(root/filename).read_text()
    out.extend(function(source,name) for name in names)
Path(sys.argv[1]).write_text('\n'.join(out))
source=(root/'src/pc/rocket_runtime.cpp').read_text().replace('extern "C" ', '')
Path(sys.argv[2]).write_text('\n'.join(function(source,name) for name in
    ['rocket_runtime_read_selected_input','rocket_runtime_read_input']))
