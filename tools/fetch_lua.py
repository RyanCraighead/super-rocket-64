"""Fetch the public upstream Lua build dependency, validating cached bytes first."""
import hashlib
from pathlib import Path
import urllib.request

root = Path(__file__).resolve().parents[1]
target = root / 'lib/lua/win64/liblua53.a'
digest = 'ee966401b62c94bb0bc72c0edd0199473912c337e497aee66b02f5d193993524'
url = 'https://raw.githubusercontent.com/coop-deluxe/sm64coopdx/8cd6e5977d9f920d51ca71f2c61801d019ed79c6/lib/lua/win64/liblua53.a'
if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == digest:
    print('Verified cached Lua 5.3 build library')
else:
    with urllib.request.urlopen(url, timeout=45) as response:
        data = response.read(1024 * 1024)
    if len(data) != 315144 or hashlib.sha256(data).hexdigest() != digest:
        raise ValueError('Official pinned Lua library failed size/SHA-256 validation')
    target.parent.mkdir(parents=True, exist_ok=True)
    temp = target.with_suffix('.download')
    temp.write_bytes(data)
    temp.replace(target)
    print('Verified official pinned Lua 5.3 build library')
