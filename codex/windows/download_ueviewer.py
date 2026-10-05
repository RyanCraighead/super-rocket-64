"""Explicit opt-in download of the pinned upstream extraction tool; no installer.

The executable is not redistributed in our ZIP. Its upstream root MIT notice
does not replace separate dependency terms. Sources/notices are available at
the pinned upstream repository. No credentials, game data or account is used.
"""
import hashlib
import uuid
from pathlib import Path
import tempfile
import urllib.request

COMMIT = 'a0bfb468d42be831b126632fd8a0ae6b3614f981'
BASE = 'https://raw.githubusercontent.com/gildor2/UEViewer/' + COMMIT + '/'
FILES = {
    'umodel.exe': '13502e5a4d8f6b5f32252afebd6360f7302ccfaccf6b8dda65beff0be2d364a0',
    'SDL2.dll': 'ffbf5aa7d13fed5d12ba68ba3af930a15aa5d0ff97cfb50a5965a498a941a6dd',
    'Unreal/UnrealPackage/UnPackageReader.cpp': '8370968ffd2fca369b934167fc89615b2de614e3ce40eb229efd9046cd1e7694',
    'LICENSE.txt': '18373196d85cd082029400869f6e9ac991c01e4aeeb575dcaa994d5a3a6679f3',
    'readme.txt': '1d6261824fa8bbabbb5fd6004596bd755f48d6efe00aff3383b29213e011d124',
}
SOURCE_NAMES = {'SDL2.dll': 'libs/SDL2/x86/SDL2.dll'}
# The original audited Windows checkout used CRLF for these two text files.
# Verify immutable Git bytes first, then preserve that known checkout profile.
SOURCE_HASHES = {
    'Unreal/UnrealPackage/UnPackageReader.cpp': '72e7700ea05b937b473451e05db873b8eb8dd9e4b41c329d6e9a0d2e7e56132d',
    'LICENSE.txt': '23831a13a99f726e62388efade7f0b8a778f1b84e40dec8a4bf4a1e481ccbdfe',
}


def verify(directory):
    for name, expected in FILES.items():
        path = Path(directory) / name
        if name in SOURCE_NAMES and not path.exists():
            path = Path(directory) / SOURCE_NAMES[name]
        if not path.is_file() or path.is_symlink() or path.stat().st_size > 8 * 1024 * 1024:
            raise ValueError('Missing/invalid pinned UE Viewer file: ' + name)
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('Pinned UE Viewer hash mismatch: ' + name)
    return Path(directory)


def download(directory, *, consent=False, cancel_check=lambda: None):
    if not consent:
        raise ValueError('Explicit download consent is required')
    directory = Path(directory)
    cancel_check()
    if directory.exists():
        try:
            return verify(directory)
        except ValueError:
            # Preserve failed cache for diagnosis; publish a fresh verified cache atomically.
            directory.rename(directory.with_name(directory.name + '.invalid-' + uuid.uuid4().hex))
    directory.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ueviewer-download-', dir=directory.parent) as temporary:
        stage = Path(temporary) / 'tool'
        stage.mkdir()
        for name, expected in FILES.items():
            cancel_check()
            request = urllib.request.Request(BASE + SOURCE_NAMES.get(name, name), headers={'User-Agent': 'SuperRocket64-LocalAssetSetup/1'})
            with urllib.request.urlopen(request, timeout=10) as response:
                if not response.url.startswith('https://raw.githubusercontent.com/'):
                    raise ValueError('Unexpected tool download redirect')
                chunks = []
                total = 0
                while True:
                    cancel_check()
                    chunk = response.read(min(65536, 8 * 1024 * 1024 + 1 - total))
                    if not chunk:
                        break
                    chunks.append(chunk)
                    total += len(chunk)
                    if total > 8 * 1024 * 1024:
                        raise ValueError('Tool download exceeds expected size bound: ' + name)
                data = b''.join(chunks)
            if len(data) > 8 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != SOURCE_HASHES.get(name, expected):
                raise ValueError('Tool download failed pinned hash check: ' + name)
            if name in SOURCE_HASHES:
                data = data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError('Tool checkout representation failed hash check: ' + name)
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        verify(stage)
        cancel_check()
        stage.rename(directory)
    return directory
