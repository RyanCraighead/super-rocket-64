"""Use exact native standard-action bodies for terminal collision ordering."""
from pathlib import Path
import sys
from extract_enemy_native import extract

if __name__ == "__main__":
    names = ["approach_f32_ptr", "obj_act_squished", "obj_update_standard_actions"]
    Path(sys.argv[1]).write_text("\n".join(
        extract("src/game/obj_behaviors_2.c", name, False) for name in names))
