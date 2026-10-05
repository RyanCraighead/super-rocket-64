"""Shared engine data from original source components and a locally owned ROM."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shutil
import struct
import uuid
import launcher as common
import owned_audio_setup as audio
import owned_ctl_setup as ctl
import custom_visual_setup as visual
import source_audio_setup as source_audio

RECEIPT = 'engine-setup.json'
RECIPE = 'codex/windows/engine_recipe.json'
AUDIO_RECIPE = 'codex/windows/owned-audio-recipe.json'


def bank_sets(metadata):
    slots = metadata['sequences']
    if not isinstance(slots, list) or len(slots) != 35:
        raise ValueError('Invalid engine bank metadata.')
    body = bytearray()
    offsets = bytearray()
    for banks in slots:
        if not isinstance(banks, list) or len(banks) > 255 or any(type(x) is not int or not 0 <= x < 45 for x in banks):
            raise ValueError('Invalid engine bank references.')
        offsets.extend(struct.pack('<H', 2 * len(slots) + len(body)))
        body.append(len(banks))
        body.extend(reversed(banks))
    result = offsets + body
    result.extend(b'\0' * (-len(result) % 16))
    return bytes(result)


def recipe(root):
    path = common.private_path(root, RECIPE)
    raw = common.read_bounded(path, 1024 * 1024)
    value = json.loads(raw)
    common.require(isinstance(value, dict) and value.get('format') == 'sr64-engine-setup@1' and
                   isinstance(value.get('files'), dict) and len(value['files']) == 115, 'Unsupported engine setup recipe.')
    for name, entry in value['files'].items():
        common.private_path(root, name)
        common.require(isinstance(entry, dict) and type(entry.get('size')) is int and
                       0 < entry['size'] <= 16 * 1024 * 1024 and entry.get('method') in ('bank_sets', 'owned_ctl', 'source_png', 'source_array', 'source_music', 'source_samples'),
                       'Invalid engine data recipe entry.')
        digest = entry.get('sha256', '')
        common.require(isinstance(digest, str) and len(digest) == 64 and all(c in '0123456789abcdef' for c in digest),
                       'Invalid engine data digest.')
    audio_raw = common.read_bounded(common.private_path(root, AUDIO_RECIPE), 1024 * 1024)
    common.require(hashlib.sha256(audio_raw).hexdigest() == value['owned_audio_recipe_sha256'], 'Owned audio recipe changed; repair the installer.')
    return value, hashlib.sha256(raw).hexdigest()


def target(root, data):
    value, identity = recipe(root)
    return common.private_path(data, '.runtime/engine-data/' + identity[:20]), value, identity


def valid(directory, value, identity):
    try:
        receipt = json.loads(common.read_bounded(common.private_path(directory, RECEIPT), 65536))
        if receipt != {'format': 'sr64-engine-cache@1', 'recipe_sha256': identity}:
            return False
        for name, entry in value['files'].items():
            path = common.private_path(directory, name)
            if not path.is_file() or path.stat().st_size != entry['size'] or common.sha256(path) != entry['sha256']:
                return False
        return True
    except (ValueError, OSError, KeyError, TypeError):
        return False


def require_ready(root, data):
    directory, value, identity = target(root, data)
    common.require(valid(directory, value, identity), 'Shared game data is missing or changed. Run Setup again; existing saves and character assets will be retained.')
    return directory


def prepare(root, data, rom, cancel_check=lambda: None):
    root, data = Path(root), Path(data)
    cancel_check()
    directory, value, identity = target(root, data)
    audio_path = common.private_path(root, AUDIO_RECIPE)
    audio_value, audio_id = audio.load_recipe(audio_path)
    audio_cache = common.private_path(data, '.runtime/owned-audio/' + audio_id[:20])
    if valid(directory, value, identity) and audio.verify_cache(audio_cache, audio_value, audio_id):
        return {'status': 'reused', 'directory': str(directory), 'complete_engine_profile': True,
                'public_release_ready': bool(value.get('public_release_ready', False)), 'recipe_sha256': identity}
    runtime = common.private_path(data, '.runtime')
    runtime.mkdir(parents=True, exist_ok=True)
    common.require(not any(runtime.glob('**/.launch-lock')), 'Close the running game before repairing shared game data.')
    lock = common.private_path(data, '.runtime/.engine-setup-lock')
    try:
        lock.mkdir()
    except FileExistsError as error:
        raise common.SetupError('Another shared-data setup may be running. Close it before retrying; do not remove a live setup lock.') from error
    stage = None
    backup = None
    try:
        def audio_cancel():
            cancel_check()
            return False
        owned_result = audio.prepare(audio_path, audio_cache, rom, audio_cancel)
        if valid(directory, value, identity):
            return {'status': 'reused', 'directory': str(directory), 'owned_audio': owned_result['status'],
                    'complete_engine_profile': True, 'public_release_ready': bool(value.get('public_release_ready', False)), 'recipe_sha256': identity}
        directory.parent.mkdir(parents=True, exist_ok=True)
        stage = directory.with_name('.stage-' + uuid.uuid4().hex)
        stage.mkdir()
        packed = {group: (audio_cache / filename).read_bytes() for group, filename in audio.FILES.items()}
        for name, entry in value['files'].items():
            cancel_check()
            if entry['method'] == 'bank_sets':
                payload = bank_sets(value['bank_sets'])
            elif entry['method'] == 'source_music':
                try:
                    prefix = common.private_path(root, entry['source_input'])
                    payload = source_audio.source_music(prefix, audio_value['groups']['music'], packed['music'], entry['size'], entry['sha256'], cancel_check)
                except (ValueError, RuntimeError) as error:
                    cancel_check()
                    raise common.SetupError('Music setup failed: ' + str(error) + '. Restore the matching installer and retry; existing saves are retained.') from error
            elif entry['method'] == 'source_samples':
                print('Assembling and verifying audio from the original custom recordings and your ROM...', flush=True)
                audio_work = common.private_path(stage, '.audio-work')
                audio_work.mkdir()
                try:
                    payload, control = source_audio.source_samples(root, rom, audio_work, audio_value['groups']['samples'], packed['samples'], entry['size'], entry['sha256'], cancel_check)
                    common.require(hashlib.sha256(control).hexdigest() == value['files']['sound/sound_data.ctl']['sha256'], 'Source audio produced inconsistent control metadata.')
                except (ValueError, RuntimeError, SystemExit) as error:
                    cancel_check()
                    raise common.SetupError('Audio setup failed: ' + str(error) + '. Restore the matching installer and retry; existing saves are retained.') from error
                finally:
                    common.require(audio_work.resolve().is_relative_to(stage.resolve()) and audio_work.name == '.audio-work', 'Refusing unsafe audio staging cleanup.')
                    shutil.rmtree(audio_work)
            elif entry['method'] in ('source_png', 'source_array'):
                common.private_path(root, entry['source_input'])
                try:
                    payload = visual.generate(root, entry, cancel_check)
                except ValueError as error:
                    raise common.SetupError('Visual setup failed: ' + str(error) + '. Restore the matching installer and retry; existing saves are retained.') from error
            elif entry['method'] == 'owned_ctl':
                print('Rebuilding and verifying sound-control metadata from your local inputs...', flush=True)
                ctl_work = common.private_path(stage, '.ctl-work')
                ctl_work.mkdir()
                try:
                    payload = ctl.generate(root, rom, ctl_work, cancel_check)
                except (RuntimeError, SystemExit) as error:
                    cancel_check()
                    raise common.SetupError('Sound-control setup failed: ' + str(error) + '. Restore the matching installer and retry; existing saves are retained.') from error
                finally:
                    common.require(ctl_work.resolve().is_relative_to(stage.resolve()) and ctl_work.name == '.ctl-work', 'Refusing unsafe control-metadata cleanup.')
                    shutil.rmtree(ctl_work)
            else:
                raise common.SetupError('Unsupported source-generation method.')
            common.require(len(payload) == entry['size'] and hashlib.sha256(payload).hexdigest() == entry['sha256'], 'Assembled game data failed verification: ' + name)
            path = common.private_path(stage, name)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
        common.private_path(stage, RECEIPT).write_text(json.dumps({'format': 'sr64-engine-cache@1', 'recipe_sha256': identity}) + '\n', encoding='utf-8')
        common.require(valid(stage, value, identity), 'Staged game data failed verification.')
        cancel_check()
        if directory.exists():
            common.require(not common.is_redirect(directory), 'Shared data must not be a link or junction.')
            backup = directory.with_name(directory.name + '.before-repair-' + uuid.uuid4().hex)
            directory.rename(backup)
        try:
            stage.rename(directory)
            stage = None
        except BaseException:
            if backup is not None:
                backup.rename(directory)
            raise
        return {'status': 'prepared', 'directory': str(directory), 'owned_audio': owned_result['status'],
                'recovery_copy': str(backup) if backup else None, 'complete_engine_profile': True,
                'public_release_ready': bool(value.get('public_release_ready', False)), 'recipe_sha256': identity}
    finally:
        if stage is not None and stage.exists():
            # The only recursive cleanup is this operation's verified staging tree.
            resolved = stage.resolve()
            common.require(resolved != runtime.resolve() and resolved.is_relative_to(runtime.resolve()) and stage.name.startswith('.stage-'), 'Refusing unsafe setup cleanup.')
            shutil.rmtree(stage)
        lock.rmdir()
