"""Persistent input preferences shared by this installation's play modes."""
from __future__ import annotations
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import launcher as old

LIMIT = 65536

def records(data, defaults, strict=False):
    result = {}
    for line in data.decode('utf-8-sig').splitlines():
        parts = line.split()
        if not parts or parts[0].startswith('#') or parts[0] not in defaults:
            continue
        key, values = parts[0], parts[1:]
        valid = True
        if key == 'rocket-bindings:':
            valid = len(values) == 12 and values[0] == '1' and all(v.isascii() and v.isdecimal() for v in values)
            valid = valid and all(int(v) < 15 for v in values[1:9]) and all(int(v) < 2 for v in values[9:])
            if valid: values = [str(int(v)) for v in values]
        elif key.startswith('key_'):
            try:
                valid = 1 <= len(values) <= 3 and all(0 <= int(v, 16) <= 65535 and not v.startswith(('-', '+')) for v in values)
                if valid: values = [format(int(v, 16), '04x') for v in values] + defaults[key][len(values):]
            except ValueError:
                valid = False
        elif defaults[key] in (['true'], ['false']):
            valid = len(values) == 1 and values[0] in ('true', 'false', '0', '1')
            if valid: values = ['true' if values[0] in ('true', '1') else 'false']
        else:
            valid = len(values) == 1 and values[0].isascii() and values[0].isdecimal() and int(values[0]) <= 0xffffffff
            if valid: values = [str(int(values[0]))]
            if key == 'rocket_camera_mode': valid = valid and int(values[0]) <= 1
        if not valid:
            old.require(not strict, 'Invalid saved control entry: ' + key + '. Restore its backup or correct that entry; existing files were kept.')
            continue
        result[key] = values
    return result

def read(path):
    return old.read_bounded(path, LIMIT)

def defaults_from_engine(engine):
    result = subprocess.run([str(engine), '--controls-defaults'], capture_output=True, timeout=30,
                            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0), check=False)
    old.require(result.returncode == 0 and 0 < len(result.stdout) <= LIMIT,
                'Could not read this game build\'s control defaults. Repair the installation and retry.')
    defaults = {}
    for line in result.stdout.decode('utf-8').splitlines():
        parts = line.split()
        if parts: defaults[parts[0]] = parts[1:]
    old.require('rocket-bindings:' in defaults and 'key_a' in defaults and 'key_b' in defaults,
                'The game did not return its control settings. Repair the installation and retry.')
    # These were the launcher's original first-run camera recommendations.
    defaults['bettercam_enable'] = ['true']
    defaults['bettercam_analog'] = ['true']
    defaults['background_gamepad'] = ['false']
    return defaults

def directory(data):
    target = old.private_path(data, '.runtime/controls')
    target.mkdir(parents=True, exist_ok=True)
    return target

@contextmanager
def locked(data, saves, active_save):
    target = directory(data)
    for save in saves:
        if save != active_save:
            old.require(not old.private_path(save, '.launch-lock').exists(),
                        'Another game is using this installation. Close it before changing play modes.')
    lock = old.private_path(target, '.launch-lock')
    try:
        lock.mkdir()
    except FileExistsError as error:
        raise old.SetupError('Controls may be in use. Close the other game; after a crash remove only the empty lock: ' + str(lock)) from error
    try:
        yield target
    finally:
        lock.rmdir()

def prepare(data, engine, saves, active_save):
    target = directory(data)
    config = old.private_path(target, 'controls.cfg')
    for name in ('controls.cfg.tmp', 'controls.cfg.backup', 'controls.cfg.backup.tmp', 'imported-controls.cfg', 'migration.json'):
        old.private_path(target, name)
    # Existing shared controls are authoritative. Validate with the same engine
    # parser before starting a game; never launch with silently reset controls.
    if config.exists():
        old.require(config.is_file() and config.stat().st_size <= LIMIT, 'Saved controls are not a valid settings file: ' + str(config))
        result = subprocess.run([str(engine), '--validate-controls', str(config)], capture_output=True, timeout=30,
                                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0), check=False)
        old.require(result.returncode == 0, 'Saved controls are unreadable or invalid: ' + str(config) +
                    '. Restore controls.cfg.backup or correct the file, then retry. Existing files were kept.')
        return config
    defaults = defaults_from_engine(engine)
    candidates = []
    for index, save in enumerate(dict.fromkeys(saves)):
        primary = old.private_path(save, 'sm64config.txt')
        backup = old.private_path(save, 'sm64config-backup.txt')
        source = primary if primary.is_file() else backup
        if not source.is_file(): continue
        raw = read(source)
        parsed = records(raw, defaults)
        candidates.append((source.stat().st_mtime_ns, save == active_save, index, source, raw, parsed))
    chosen = {key: list(values) for key, values in defaults.items()}
    origins = {}
    for key, base in defaults.items():
        # Treat keyboard/gamepad/mouse slots and car actions independently, so
        # a custom boost in one old mode cannot erase a custom jump in another.
        for slot in range(len(base)):
            choices = [(values[key][slot] != base[slot], stamp, active, -index, values[key][slot], source)
                       for stamp, active, index, source, raw, values in candidates if key in values]
            if choices:
                custom, stamp, active, index, value, source = max(choices)
                chosen[key][slot] = value
                if custom: origins[key + ':' + str(slot)] = str(source)
    output = '# Super Rocket 64 shared controls\n' + ''.join(key + ' ' + ' '.join(values) + '\n' for key, values in chosen.items())
    records(output.encode(), defaults, strict=True)
    # Stage all backups/evidence first. No legacy profile is rewritten, and a
    # failed migration never leaves a partial authoritative controls file.
    with tempfile.TemporaryDirectory(prefix='.controls-import-', dir=target) as temporary:
        stage = Path(temporary)
        entries = []
        for stamp, active, index, source, raw, values in candidates:
            name = 'profile-' + str(index) + '-' + hashlib.sha256(raw).hexdigest()[:16] + '.cfg'
            (stage / name).write_bytes(raw)
            entries.append({'source': str(source), 'backup': name, 'sha256': hashlib.sha256(raw).hexdigest()})
        (stage / 'controls.cfg').write_text(output, encoding='utf-8', newline='\n')
        (stage / 'migration.json').write_text(json.dumps({'schema': 1, 'sources': entries, 'custom_origins': origins}, indent=2) + '\n', encoding='utf-8')
        backup_dir = old.private_path(target, 'legacy-backups')
        backup_dir.mkdir(exist_ok=True)
        for entry in entries:
            dest = old.private_path(backup_dir, entry['backup'])
            if not dest.exists(): shutil.copyfile(stage / entry['backup'], dest)
        for name in ('migration.json', 'imported-controls.cfg'):
            dest = old.private_path(target, name)
            if not dest.exists(): shutil.copyfile(stage / ('controls.cfg' if name == 'imported-controls.cfg' else name), dest)
        old.require(not config.exists(), 'Controls were created by another launch; retry.')
        os.replace(stage / 'controls.cfg', config)
    return config
