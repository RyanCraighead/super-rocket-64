"""Prepare verbatim native-source fixtures; no ROM/assets or save files used."""
from pathlib import Path
import sys
from native_slice import function, slice_source

def extract(out):
    repo = Path(__file__).resolve().parents[3]
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    source = (repo/'src/game/mario.c').read_text()
    # These health/cap functions use no sockets. The public Windows source
    # intentionally excludes Linux transport headers from this fixture host.
    source = source.replace('#include "pc/network/socket/socket.h"', '')
    (out/'metal_health_native.inc').write_text(slice_source(source,
        ['update_mario_health','update_and_return_cap_flags','mario_update_hitbox_and_cap_model'],['sCapFlickerFrames']))
    source = (repo/'src/game/interaction.c').read_text()
    names = ['get_mario_cap_flag','mario_obj_angle_to_object','determine_interaction','attack_object',
        'determine_knockback_action','take_damage_from_interact_object','take_damage_and_knock_back',
        'bounce_back_from_attack','hit_object_from_below','bounce_off_object','mario_stop_riding_object',
        'mario_drop_held_object','mario_stop_riding_and_holding','interact_damage','interact_flame',
        'interact_shock','interact_clam_or_bubba','interact_whirlpool','interact_cap','interact_breakable','interact_star_or_key']
    (out/'metal_interaction_native.inc').write_text(slice_source(source,names,
        ['gInteractionInvulnerable','sDelayInvincTimer','sForwardKnockbackActions','sBackwardKnockbackActions','gLastCollectedStarOrKey']))
    source = (repo/'src/game/behavior_actions.c').read_text()
    cap = (repo/'src/game/behaviors/capswitch.inc.c').read_text()
    box = (repo/'src/game/behaviors/exclamation_box.inc.c').read_text()
    purple = (repo/'src/game/behaviors/purple_switch.inc.c').read_text()
    text = slice_source(source,[],['D_8032F0C0'])+'\n#define o gCurrentObject\nstatic u8 capSwitchForcePress = FALSE;\n'
    text += function(cap,'cap_switch_act_1')+function(box,'exclamation_box_act_0')+function(purple,'bhv_purple_switch_loop')
    (out/'metal_progression_native.inc').write_text(text)
    source = (repo/'src/game/mario_actions_submerged.c').read_text()
    (out/'metal_water_native.inc').write_text(slice_source(source,
        ['swimming_near_surface','get_buoyancy','apply_water_current','perform_water_step','stationary_slow_down'],
        ['sWaterCurrentSpeeds']))

if __name__ == '__main__':
    extract(sys.argv[1])
