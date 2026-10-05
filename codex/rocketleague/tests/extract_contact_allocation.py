"""Compile the actual native allocation lifetime boundary in a small fixture."""
from pathlib import Path
import re
import sys
source = Path(__file__).resolve().parents[3] / 'src/game/spawn_object.c'
text = source.read_text(encoding='utf-8')
matches = list(re.finditer(r'^struct Object \*allocate_object\([^;]*?\) \{.*?^}', text, re.M | re.S))
if len(matches) != 1:
    raise RuntimeError('Expected exactly one native allocate_object definition')
match = matches[0]
line = text[:match.start()].count('\n') + 1
Path(sys.argv[1]).write_text(f'#line {line} "src/game/spawn_object.c"\n{match[0]}\n', encoding='utf-8')
