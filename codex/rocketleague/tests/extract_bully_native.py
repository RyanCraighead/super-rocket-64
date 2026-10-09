from pathlib import Path
import sys
from native_slice import function
root=Path(__file__).resolve().parents[3]
groups={
    'src/game/mario_step.c': ['transfer_bully_speed','init_bully_collision_data'],
    'src/game/obj_behaviors.c': ['turn_obj_away_from_surface','obj_find_wall','turn_obj_away_from_steep_floor',
        'calc_obj_friction','calc_new_obj_vel_and_pos_y','obj_update_pos_vel_xz','object_step',
        'obj_check_floor_death','obj_lava_death'],
    'src/game/behaviors/bully.inc.c': ['bully_check_mario_collision','bully_act_knockback',
        'bully_backup_check','bully_play_stomping_sound','bully_step','bully_spawn_coin','bully_act_level_death'],
}
output=[line for line in (root/'src/game/obj_behaviors.c').read_text().splitlines()
        if line.startswith('#define OBJ_COL_')]
for filename,names in groups.items():
    # This test uses AVOID_UB, where the native BAD_RETURN(s32) macro is void.
    source=(root/filename).read_text().replace('BAD_RETURN(s32)', 'void')
    output.extend(function(source,name) for name in names)
Path(sys.argv[1]).write_text('\n'.join(output))
