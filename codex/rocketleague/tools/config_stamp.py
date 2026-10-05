"""Make only recompiles the optional integration when its defines change."""
from pathlib import Path
import sys
path=Path(sys.argv[1])
value=f'ROCKET_CAR={sys.argv[2]}\nROCKET_CAR_QA={sys.argv[3]}\n'
if not path.exists() or path.read_text()!=value:
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(value.encode('ascii'))
