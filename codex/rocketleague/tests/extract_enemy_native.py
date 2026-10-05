"""Compile exact native function/table bodies with explicit inert host services.

No copied implementation: fail if a named source definition is absent/ambiguous.
Unrelated behavior functions normally share these very large translation units.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
FUNCTIONS = {
    "src/game/object_helpers.c": ["obj_set_hitbox", "obj_mark_for_deletion",
        "whomp_send_native_coin", "obj_spawn_loot_coins", "obj_spawn_loot_yellow_coins", "cur_obj_set_hitbox_and_die_if_attacked"],
    "src/game/obj_behaviors_2.c": ["obj_die_if_health_non_positive", "obj_set_knockback_action",
        "obj_handle_attacks", "obj_check_attacks", "obj_act_knockback"],
    "src/game/obj_behaviors.c": ["obj_spawn_yellow_coins"],
    "src/game/behaviors/bobomb.inc.c": ["bobomb_spawn_coin", "bobomb_act_explode", "bobomb_kick_launch", "bobomb_check_interactions"],
    "src/game/behaviors/goomba.inc.c": ["mark_goomba_as_dead"],
}
TABLES = {
    "src/game/behaviors/goomba.inc.c": ["sGoombaHitbox", "sGoombaAttackHandlers"],
    "src/game/behaviors/bobomb.inc.c": ["sBobombHitbox"],
    "src/game/behaviors/spindrift.inc.c": ["sSpindriftHitbox"],
    "src/game/behaviors/scuttlebug.inc.c": ["sScuttlebugHitbox"],
    "src/game/behaviors/skeeter.inc.c": ["sSkeeterHitbox"],
    "src/game/behaviors/snufit.inc.c": ["sSnufitHitbox"],
    "src/game/behaviors/fly_guy.inc.c": ["sFlyGuyHitbox"],
}

def extract(path, name, table):
    text = (ROOT / path).read_text()
    pattern = (rf"^(?:static )?(?:struct ObjectHitbox|u8) {name}\b[^;]*?^}};" if table else
               rf"^(?:static )?(?:void|s32) {name}\([^;]*?\) \{{.*?^}}")
    matches = list(re.finditer(pattern, text, re.M | re.S))
    if len(matches) != 1:
        raise RuntimeError(f"Expected exactly one native definition: {path}:{name}")
    match = matches[0]
    line = text[:match.start()].count("\n") + 1
    return f'#line {line} "{path}"\n{match[0]}\n'

if __name__ == "__main__":
    contents = []
    for definitions, table in [(TABLES, True), (FUNCTIONS, False)]:
        for path, names in definitions.items():
            contents.extend(extract(path, name, table) for name in names)
    Path(sys.argv[1]).write_text("\n".join(contents))
