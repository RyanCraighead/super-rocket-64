from pathlib import Path
import os, subprocess, sys
from native_slice import function
root=Path(__file__).resolve().parents[3]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
sources={
 'save':('src/game/save_file.c',['bswap_signature','bswap_savefile','write_eeprom_savefile','calc_checksum','add_save_block_signature','save_file_exists','save_file_set_flags','save_file_do_save']),
 'npc':('src/game/mario_actions_cutscene.c',['mario_ready_to_speak','should_start_or_continue_dialog','set_mario_npc_dialog','act_reading_npc_dialog']),
 'move':('src/game/object_helpers.c',['dist_between_objects','obj_angle_to_object','approach_s16_symmetric','abs_angle_diff','obj_turn_toward_object','cur_obj_rotate_yaw_toward','cur_obj_compute_vel_xz','cur_obj_move_using_vel_and_gravity','cur_obj_move_using_fvel_and_gravity','cur_obj_update_dialog_with_cutscene']),
 'approach':('src/game/obj_behaviors_2.c',['obj_compute_vel_from_move_pitch','obj_turn_pitch_toward_mario','approach_f32_ptr','obj_move_pitch_approach']),
 'lakitu':('src/game/behaviors/camera_lakitu.inc.c',['bhv_camera_lakitu_ignore_if_true','bhv_camera_lakitu_override_ownership','bhv_camera_lakitu_on_received_post','bhv_camera_lakitu_init','camera_lakitu_intro_act_show_dialog_continue_dialog','camera_lakitu_intro_act_trigger_cutscene','camera_lakitu_intro_act_spawn_cloud','camera_lakitu_intro_act_show_dialog']),
 'camera':('src/game/camera.c',['cutscene_object_with_dialog','cutscene_dialog_create_dialog_box']),
 'dialog':('src/game/ingame_menu.c',['create_dialog_box']),
 'level':('src/game/level_update.c',['fake_lvl_init_from_save_file']),
}
for key,(name,names) in sources.items():
 ref=os.environ.get('WELCOME_BASELINE') if key=='lakitu' else None
 text=subprocess.check_output(['git','-c','safe.directory='+str(root),'show',ref+':'+name],cwd=root).decode() if ref else (root/name).read_text()
 if key=='camera':text=text.replace('BAD_RETURN(s32)','void') # native PC signature macro
 (out/(key+'.inc.c')).write_text('\n'.join(function(text,n) for n in names))
