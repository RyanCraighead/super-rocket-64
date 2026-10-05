"""Copy exact native function bodies into a windowless fixture (no logic edits)."""
from pathlib import Path
from native_slice import slice_source
import sys
repo=Path(__file__).resolve().parents[3]
parts=[
    ('src/engine/math_util.c',['sins','coss','atan2_lookup','atan2s'],[]),
    ('src/game/mario.c',['mario_get_floor_class','mario_floor_is_slippery'],[]),
    ('src/game/mario_step.c',['mario_update_moving_sand','mario_update_windy_ground','apply_vertical_wind'],['sMovingSandSpeeds']),
    ('src/game/mario_actions_submerged.c',['apply_water_current'],['sWaterCurrentSpeeds']),
]
def prepare(destination):
    code='/* Generated verbatim from the native sources; do not commit. */\n'
    for path,names,variables in parts:
        code+=slice_source((repo/path).read_text(),names,variables)
    Path(destination).write_text(code)
if __name__=='__main__':prepare(sys.argv[1])
