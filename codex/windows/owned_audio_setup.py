"""Internal setup component: prepare verified ROM audio overlays, not a full engine pack.

Uses only Python's bundled standard library. The caller supplies the normalized,
validated US ROM or reuses an already verified cache. No downloads or subprocesses.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import uuid

FILES = {'music': 'music-owned.bin', 'samples': 'samples-owned.bin'}
RECEIPT = 'owned-audio.json'
ROM_SHA1 = '9bef1128717f958171a4afac3ed78ee2bb4e86ce'
ROM_SIZE = 8388608


class SetupCancelled(Exception):
    pass


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def load_recipe(path):
    raw = Path(path).read_bytes()
    if len(raw) > 1024 * 1024:
        raise ValueError('Owned-audio recipe is too large.')
    recipe = json.loads(raw)
    if not isinstance(recipe, dict) or recipe.get('format') != 'sr64-owned-audio@1' or not isinstance(recipe.get('groups'), dict) or set(recipe['groups']) != set(FILES):
        raise ValueError('Unsupported owned-audio recipe.')
    if recipe.get('rom_sha1') != ROM_SHA1 or recipe.get('rom_size') != ROM_SIZE:
        raise ValueError('Owned-audio recipe has an unsupported ROM profile.')
    for name, group in recipe['groups'].items():
        if not isinstance(group, dict) or type(group.get('target_size')) is not int or not 0 < group['target_size'] <= 16 * 1024 * 1024:
            raise ValueError('Invalid owned-audio runtime buffer size.')
        entries = group.get('ranges', [])
        if not isinstance(entries, list) or not 1 <= len(entries) <= 1024:
            raise ValueError('Invalid owned-audio range count.')
        cursor = 0
        previous_end = 0
        for item in entries:
            if not isinstance(item, dict):
                raise ValueError('Invalid owned-audio range record.')
            keys = ('rom_offset', 'destination_offset', 'size', 'packed_offset')
            if any(type(item.get(k)) is not int or item[k] < 0 for k in keys):
                raise ValueError('Invalid owned-audio range.')
            start, size = item['rom_offset'], item['size']
            if size == 0 or start > ROM_SIZE or size > ROM_SIZE - start:
                raise ValueError('Owned-audio range is outside the supported ROM.')
            if item['packed_offset'] != cursor or item['destination_offset'] < previous_end:
                raise ValueError('Owned-audio ranges overlap or are not ordered.')
            if item['destination_offset'] + size > group['target_size']:
                raise ValueError('Owned-audio range is outside its runtime buffer.')
            cursor += size
            previous_end = item['destination_offset'] + size
        if cursor != group['bytes'] or cursor > 16 * 1024 * 1024:
            raise ValueError('Owned-audio packed size mismatch.')
        digest = group.get('sha256', '')
        if len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('Invalid owned-audio output digest.')
    return recipe, sha256(raw)


def verify_cache(directory, recipe, recipe_id):
    directory = Path(directory)
    try:
        receipt = json.loads((directory / RECEIPT).read_text(encoding='utf-8'))
        if receipt != {'format': 'sr64-owned-audio-cache@1', 'recipe_sha256': recipe_id}:
            return False
        for group, filename in FILES.items():
            path = directory / filename
            if path.stat().st_size != recipe['groups'][group]['bytes']:
                return False
            if sha256(path.read_bytes()) != recipe['groups'][group]['sha256']:
                return False
        return True
    except (OSError, ValueError, TypeError):
        return False


def prepare(recipe_path, directory, rom=None, cancel_check=lambda: False):
    recipe, recipe_id = load_recipe(recipe_path)
    directory = Path(directory).absolute()
    if cancel_check():
        raise SetupCancelled('Owned-audio setup canceled before extraction.')
    if verify_cache(directory, recipe, recipe_id):
        return {'status': 'reused', 'recipe_sha256': recipe_id, 'bytes': sum(g['bytes'] for g in recipe['groups'].values()), 'complete_engine_profile': False}
    if not rom:
        raise ValueError('Owned SM64 audio cache is missing or changed. Choose the supported US SM64 ROM to create or repair it.')
    rom = Path(rom)
    if not rom.is_file() or rom.stat().st_size != ROM_SIZE:
        raise ValueError('Expected the supported 8 MiB US SM64 ROM after byte-order normalization.')
    source = rom.read_bytes()
    if hashlib.sha1(source).hexdigest() != ROM_SHA1:
        raise ValueError('The SM64 ROM does not match the supported US revision.')
    if directory.exists():
        try:
            old_receipt = json.loads((directory / RECEIPT).read_text(encoding='utf-8'))
            owned = old_receipt.get('format') == 'sr64-owned-audio-cache@1'
        except (OSError, ValueError, AttributeError):
            owned = False
        if not owned:
            raise ValueError('The output folder is not an owned-audio cache. Use a dedicated empty cache location.')
    directory.parent.mkdir(parents=True, exist_ok=True)
    stage = directory.with_name('.' + directory.name + '.stage-' + uuid.uuid4().hex)
    stage.mkdir()
    backup = None
    try:
        for group, filename in FILES.items():
            data = bytearray()
            for item in recipe['groups'][group]['ranges']:
                if cancel_check():
                    raise SetupCancelled('Owned-audio extraction canceled; the previous cache was preserved.')
                begin = item['rom_offset']
                data.extend(source[begin:begin + item['size']])
            if sha256(data) != recipe['groups'][group]['sha256']:
                raise ValueError('Owned-audio extraction did not match the pinned recipe.')
            with (stage / filename).open('xb') as output:
                output.write(data)
        (stage / RECEIPT).write_text(json.dumps({'format': 'sr64-owned-audio-cache@1', 'recipe_sha256': recipe_id}) + '\n', encoding='utf-8')
        if not verify_cache(stage, recipe, recipe_id):
            raise ValueError('Staged owned-audio cache failed verification.')
        if cancel_check():
            raise SetupCancelled('Owned-audio setup canceled before commit; the previous cache was preserved.')
        if directory.exists():
            backup = directory.with_name(directory.name + '.recovery-' + uuid.uuid4().hex)
            directory.rename(backup)
        try:
            stage.rename(directory)
        except OSError:
            if backup is not None:
                backup.rename(directory)
            raise
        return {'status': 'prepared', 'recipe_sha256': recipe_id, 'bytes': sum(g['bytes'] for g in recipe['groups'].values()), 'recovery_copy': str(backup) if backup else None, 'complete_engine_profile': False}
    finally:
        if stage.exists():
            # Only three filenames created by this operation; no recursive deletion.
            for filename in [*FILES.values(), RECEIPT]:
                (stage / filename).unlink(missing_ok=True)
            stage.rmdir()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recipe', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--rom')
    parser.add_argument('--cancel-file')
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.recipe, args.output, args.rom,
              lambda: bool(args.cancel_file and Path(args.cancel_file).exists()))))
        return 0
    except SetupCancelled as error:
        print(json.dumps({'status': 'canceled', 'error': str(error)}))
        return 130
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(json.dumps({'status': 'failed', 'error': str(error)}))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
