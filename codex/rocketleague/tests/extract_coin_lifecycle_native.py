"""Extract actual native allocation/area-reset bodies for coin lifetime tests."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]

def extract(path, name):
    source = (ROOT / path).read_text()
    pattern = rf"^(?:void|struct Object \*)\s*{name}\([^;]*?\) \{{.*?^}}"
    matches = list(re.finditer(pattern, source, re.M | re.S))
    if len(matches) != 1:
        raise RuntimeError(f"Expected one actual native definition: {path}:{name}")
    found = matches[0]
    line = source[:found.start()].count("\n") + 1
    return f'#line {line} "{path}"\n{found[0]}\n'

if __name__ == "__main__":
    Path(sys.argv[1]).write_text("\n".join(extract(path, name) for path, name in (
        ("src/game/spawn_object.c", "allocate_object"),
        ("src/pc/network/network.c", "network_on_init_area"),
        ("src/pc/network/sync_object.c", "sync_objects_clear"),
    )))
