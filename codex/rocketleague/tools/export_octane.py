"""Export the user's installed Octane into a private directory, never the game install.

This bounded profile repairs a changed chunk table in a COPY for pinned UE Viewer.
It does not extract physics code, launch Rocket League, or access a game process.
Uses Windows CNG (or cryptography elsewhere); pinned UE Viewer is required.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from local_aes import aes_ecb

READER_SHA = '8370968ffd2fca369b934167fc89615b2de614e3ce40eb229efd9046cd1e7694'
VIEWER_SHA = '13502e5a4d8f6b5f32252afebd6360f7302ccfaccf6b8dda65beff0be2d364a0'
BODY_SHA = 'bedf7fc0d64ab2c6d4f2620a946bb88e600f779c3e924fb80ab96c431d67f7e6'
WHEEL_SHA = '9b2582f69e6bf2fd06272b9b545dfd33cfc931f31d373b63a1078a747d902560'

def verified(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f'Unsupported source hash: {path.name}; needs a new audited profile')
    return data

def compact_chunks(plain, offset, count, package):
    """Validate every new 36-byte record, then produce the old 24-byte layout."""
    if count < 1 or count > 4096 or offset < 0 or offset + 4 + count * 36 > len(plain):
        raise ValueError('Invalid chunk table bounds')
    if struct.unpack_from('<I', plain, offset)[0] != count:
        raise ValueError('Unexpected chunk count')
    records = []
    previous_u = previous_c = None
    for index in range(count):
        pos = offset + 4 + index * 36
        u, us, c, cs = struct.unpack_from('<QIQI', plain, pos)
        if plain[pos+24:pos+36] != bytes(12):
            raise ValueError('Unrecognized extra chunk metadata')
        if not us or not cs or c + cs > len(package) or package[c:c+4] != bytes.fromhex('c1832a9e'):
            raise ValueError('Invalid compressed chunk')
        if previous_u is not None and (u != previous_u or c != previous_c):
            raise ValueError('Noncontiguous chunk table')
        previous_u, previous_c = u + us, c + cs
        records.append(struct.pack('<QIQI', u, us, c, cs))
    result = bytearray(plain)
    packed = b''.join(records)
    result[offset+4:offset+4+len(packed)] = packed
    return bytes(result)

def repair_body(package, reader):
    block = reader.decode().split('class FFileReaderRocketLeague', 1)[1]
    block = block.split('static const byte key[] = {', 1)[1].split('}', 1)[0]
    key = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', block))
    start, end = struct.unpack_from('<I', package, 29)[0], struct.unpack_from('<I', package, 8)[0]
    if len(key) != 32 or (start, end) != (0x2b9, 0x14629):
        raise ValueError('Unexpected pinned header/key layout')
    plain = aes_ecb(package[start:end], key)
    fixed = compact_chunks(plain, 0x10b0c, 17, package)
    return package[:start] + aes_ecb(fixed, key, encrypt=True) + package[end:]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True, help='Rocket League installation root')
    parser.add_argument('--ueviewer', type=Path, required=True, help='Pinned UEViewer checkout with umodel.exe')
    parser.add_argument('--out', type=Path, required=True, help='New private output directory')
    args = parser.parse_args()
    game, viewer, out = args.game.resolve(), args.ueviewer.resolve(), args.out.resolve()
    if out == game or game in out.parents or out.exists():
        raise ValueError('Output must be a new directory outside the installed game')
    cooked = game / 'TAGame/CookedPCConsole'
    reader = verified(viewer / 'Unreal/UnrealPackage/UnPackageReader.cpp', READER_SHA)
    verified(viewer / 'umodel.exe', VIEWER_SHA)
    body = verified(cooked / 'Body_Octane_SF.upk', BODY_SHA)
    verified(cooked / 'wheel_sport80_SF.upk', WHEEL_SHA)
    fixed = repair_body(body, reader)  # validate before writing anything
    out.mkdir(parents=True)
    # Distinct basename prevents UE Viewer resolving the installed, unmodified copy.
    package = out / 'CodexOctaneFixed.upk'
    package.write_bytes(fixed)
    exports = [
        (str(package), 'Body_Octane_PremiumSkin_SK', True),
        (str(package), 'Body_Octane_Bevel_RGB', False),
        ('wheel_sport80_SF.upk', 'Wheel_Sport80_SM', True),
    ]
    for name, obj, mesh in exports:
        command = [str(viewer / 'umodel.exe'), '-export', '-gltf', '-png', '-noanim', '-game=rocketleague',
                   f'-path={cooked}', f'-out={out / "export"}']
        if mesh:
            command.append('-notex')  # avoid unrelated Startup material dependencies
        result = subprocess.run(command + [name, obj], capture_output=True, text=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        log = result.stdout + result.stderr
        (out / (obj + '.log')).write_text(log, encoding='utf-8')
        if result.returncode or '*** ERROR' in log or 'is missing' in log:
            raise RuntimeError(f'Export failed; inspect {obj}.log')
    files = {str(p.relative_to(out)).replace('\\', '/'): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted((out / 'export').rglob('*')) if p.is_file()}
    report = dict(schema='octane-private-export-v1', steam_build=25535926,
                  body_sha256=BODY_SHA, wheel_sha256=WHEEL_SHA, ueviewer_reader_sha256=READER_SHA,
                  ueviewer_exe_sha256=VIEWER_SHA, chunk_record_bytes_before=36,
                  chunk_record_bytes_after=24, chunks_validated=17, files=files,
                  caveats=['Original geometry; no original physics code.',
                           'Sport80 wheels selected from the owned installation.',
                           'Original UE3 materials/paint shaders are not reproduced.'])
    (out / 'provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Exported verified original Octane geometry and texture to {out}')

if __name__ == '__main__':
    main()
