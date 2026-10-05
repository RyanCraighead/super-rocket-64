import hashlib,json,sys
from pathlib import Path
root=Path(sys.argv[1]); manifest=json.loads(Path(sys.argv[2]).read_text())
assert manifest['commit']=='c2baacb8f4b441dd8505e63c2aeb5a1679b60b02'
expected=manifest['files']
actual={p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
assert actual==set(expected), 'Unexpected or missing vendor files'
for name,digest in expected.items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest, name
print('Pinned RocketSim source verified:',len(expected),'files')
