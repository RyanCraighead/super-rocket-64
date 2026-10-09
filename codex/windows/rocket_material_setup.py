"""Optional original Octane UVs/diffuse textures from an owned installation.

Existing v1 geometry, saves, controls and audio stay byte-for-byte unchanged.
Only a new verified material directory is published; no game assets ship here.
The host uses its own lighting, not the original UE3 material/shader graph.
"""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import uuid

from codex.rocketleague.tools.export_octane import BODY_SHA, READER_SHA, VIEWER_SHA, compact_chunks, repair_body
from codex.rocketleague.tools.local_aes import aes_ecb
from codex.rocketleague.tools.convert_octane import geometry

PROFILE = json.loads(Path(__file__).with_name('rocket-material-profile.json').read_text(encoding='utf-8'))


def read(path, limit):
    path = Path(path)
    if path.is_symlink() or getattr(path, 'is_junction', lambda: False)() or not path.is_file() or path.stat().st_size > limit:
        raise ValueError('Missing/invalid material input: ' + path.name)
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Material input exceeds size limit: ' + path.name)
    return data


def verified(path, sha, limit):
    data = read(path, limit)
    if hashlib.sha256(data).hexdigest() != sha:
        raise ValueError('Unsupported or changed material input: ' + Path(path).name)
    return data


def repair_startup(package, reader):
    # Pinned owned Startup profile. Align matches the audited reader's AES
    # read window. Only its 55 validated chunk records change in a temp copy.
    if hashlib.sha256(package).hexdigest() != PROFILE['startup_package_sha256']:
        raise ValueError('Unsupported Rocket League Startup package')
    start, end = struct.unpack_from('<I', package, 29)[0], struct.unpack_from('<I', package, 8)[0]
    if (start, end) != (8821, 417661) or len(package) != 42835488:
        raise ValueError('Unexpected Startup header')
    aligned = start + ((end - start + 15) // 16) * 16
    block = reader.decode().split('class FFileReaderRocketLeague', 1)[1].split('static const byte key[] = {', 1)[1].split('}', 1)[0]
    key = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', block))
    if len(key) != 32:
        raise ValueError('Unsupported material reader')
    plain = aes_ecb(package[start:aligned], key)
    fixed = compact_chunks(plain, 393619, 55, package)
    result = package[:start] + aes_ecb(fixed, key, encrypt=True) + package[aligned:]
    # The aligned tail overlaps the first compressed block; it must be intact.
    if result[end:] != package[end:] or len(result) != len(package):
        raise ValueError('Material compatibility repair crossed the header')
    return result


def validate(directory):
    directory = Path(directory)
    if directory.is_symlink() or getattr(directory, 'is_junction', lambda: False)():
        raise ValueError('Material folder must be an ordinary directory')
    if json.loads(read(directory / 'manifest.json', 16384)) != PROFILE:
        raise ValueError('Unsupported local material profile')
    for name, record in PROFILE['files'].items():
        data = verified(directory / name, record['sha256'], record['size'])
        if len(data) != record['size']:
            raise ValueError('Invalid material size: ' + name)
    return directory


def export(game, viewer, stage, cancel_check=lambda: None):
    """Write only to a caller-owned new staging directory, using hidden CLI."""
    cooked = Path(game) / 'TAGame/CookedPCConsole'
    viewer, stage = Path(viewer), Path(stage)
    from codex.windows.download_ueviewer import verify
    verify(viewer)
    reader = verified(viewer / 'Unreal/UnrealPackage/UnPackageReader.cpp', READER_SHA, 1024 * 1024)
    verified(viewer / 'umodel.exe', VIEWER_SHA, 8 * 1024 * 1024)
    body = verified(cooked / 'Body_Octane_SF.upk', BODY_SHA, 64 * 1024 * 1024)
    startup = verified(cooked / 'Startup.upk', PROFILE['startup_package_sha256'], 64 * 1024 * 1024)
    if stage.exists() or stage.resolve() == Path(game).resolve() or Path(game).resolve() in stage.resolve().parents:
        raise ValueError('Material output must be a new folder outside the installed game')
    # Temporary copies are scoped to this operation, never installed in RL.
    with tempfile.TemporaryDirectory(prefix='sr64-materials-') as temporary:
        tmp = Path(temporary)
        packages = {'CodexOctaneFixed': repair_body(body, reader), 'CodexStartupMaterial': repair_startup(startup, reader)}
        for name, data in packages.items():
            (tmp / (name + '.upk')).write_bytes(data)
        for package, obj, mesh in [('CodexOctaneFixed', 'Body_Octane_PremiumSkin_SK', True),
                                   ('CodexStartupMaterial', 'Pepe_Body_D', False),
                                   ('CodexStartupMaterial', 'Chasis_Pepe_D', False)]:
            cancel_check()
            command = [str(viewer / 'umodel.exe'), '-export', '-gltf', '-png', '-noanim', '-game=rocketleague',
                       '-path=' + str(cooked), '-out=' + str(tmp / 'export')]
            if mesh:
                command.append('-notex')
            result = subprocess.run(command + [str(tmp / (package + '.upk')), obj], cwd=tmp,
                                    capture_output=True, timeout=60, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            log = (result.stdout + result.stderr).decode('utf-8', 'replace')
            if result.returncode or 'ERROR' in log or 'is missing' in log or 'was not found' in log:
                raise ValueError('Original material export failed: ' + obj + '\n' + log[-1800:])
        groups = geometry(tmp / 'export/CodexOctaneFixed/SkeletalMesh3/Body_Octane_PremiumSkin_SK.gltf', include_uv=True)
        if [len(g) for g in groups] != [48480, 36954]:
            raise ValueError('Unsupported original body material groups')
        original = b''.join(struct.pack('<10f', *v[:6], *color) for g, color in zip(groups, [(.12,.14,.17,1),(.025,.36,.95,1)]) for v in g)
        if hashlib.sha256(original).hexdigest() != PROFILE['body_geometry_sha256']:
            raise ValueError('Material UVs do not match the installed mesh profile')
        files = {'body.uv': b''.join(struct.pack('<2f', *v[6:]) for g in groups for v in g)}
        for target, name in [('body.png', 'Pepe_Body_D'), ('chassis.png', 'Chasis_Pepe_D')]:
            files[target] = verified(tmp / 'export/CodexStartupMaterial/Texture2D' / (name + '.png'),
                                     PROFILE['files'][target]['sha256'], PROFILE['files'][target]['size'])
        for name, data in files.items():
            if len(data) != PROFILE['files'][name]['size'] or hashlib.sha256(data).hexdigest() != PROFILE['files'][name]['sha256']:
                raise ValueError('Decoded material profile mismatch: ' + name)
        cancel_check()
        stage.mkdir(parents=True)
        for name, data in files.items():
            (stage / name).write_bytes(data)
        (stage / 'manifest.json').write_text(json.dumps(PROFILE, indent=2) + '\n', encoding='utf-8')
        validate(stage)


def prepare(game, directory, viewer, cancel_check=lambda: None):
    directory = Path(directory)
    cancel_check()
    if directory.exists():
        try:
            validate(directory)
            return 'Original car materials verified.'
        except (ValueError, OSError, KeyError, TypeError):
            pass
    if game is None:
        raise ValueError('Select the installed Rocket League folder in Setup to add original car materials.')
    # Verify the base mesh before adding UVs; cached audio/geometry never change.
    verified(directory.parent / 'body.bin', PROFILE['body_geometry_sha256'], 3417360)
    directory.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.materials-', dir=directory.parent) as temporary:
        stage = Path(temporary) / 'verified'
        export(game, viewer, stage, cancel_check)
        cancel_check()
        backup = None
        if directory.exists():
            backup = directory.with_name(directory.name + '.invalid-' + uuid.uuid4().hex)
            directory.rename(backup)
        try:
            stage.rename(directory)
        except BaseException:
            if backup is not None:
                backup.rename(directory)
            raise
    return 'Original car UVs and body/chassis textures ready; host lighting remains approximate.'
