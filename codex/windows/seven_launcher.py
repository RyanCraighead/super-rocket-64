#!/usr/bin/env python3
"""Guided local asset setup and seven-character/native network play."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(Path(__file__).parent))
import launcher as old
import engine_setup
from codex.spiderman.inspect_n64_input import read_input

CHARACTERS = ('mario', 'link', 'bomberman', 'banjo', 'spiderman', 'tony', 'octane')
ASSETS = {'link': ('oot-link', '--oot-link'), 'bomberman': ('bm64-bomberman', '--bm64-bomberman'),
          'banjo': ('bk-duo', '--bk-duo'), 'spiderman': ('spiderman-original', '--spiderman-original'),
          'tony': ('thps-original', '--thps-original'), 'octane': ('octane-model', '--rocket-car')}
DATA_ROOT = None
CANCEL_FILE = None
require = old.require


def profile(character, root=ROOT):
    if character in old.CHARACTERS:
        return old.profile_path(DATA_ROOT or root, character)
    require(character in CHARACTERS, 'Unknown character')
    return old.private_path(DATA_ROOT or root, '.runtime/windows-' + character)


def read_json(path, limit=8 * 1024 * 1024):
    return json.loads(old.read_bounded(path, limit).decode('utf-8'))


def sm64_bytes(path):
    data, _, _ = read_input(Path(path))
    data, _ = old.normalize_rom(data)
    require(len(data) == old.SM64_SIZE and hashlib.sha1(data).hexdigest() == old.SM64_SHA1,
            'Use the original SM64 US ROM (or ZIP containing only that ROM)')
    return data


def check_extra(target, character):
    inventory = read_json(old.private_path(target, 'setup.json'))
    require(isinstance(inventory, dict) and inventory.get('character') == character and isinstance(inventory.get('files'), dict), 'Invalid local setup inventory')
    require(1 <= len(inventory['files']) <= 2000, 'Invalid local setup inventory size')
    folder = ASSETS[character][0]
    for name, record in inventory['files'].items():
        require(isinstance(record, dict) and not any(c in name for c in ('\\', ':')) and
                (name == 'save/baserom.us.z64' or name.startswith(folder + '/')), 'Unexpected local asset path or record')
        path = old.private_path(target, name)
        if character == 'octane' and name.startswith('octane-model/audio/'):
            continue  # Optional sounds validate separately; never disable the car.
        require(path.is_file() and path.stat().st_size == record['size'] and old.sha256(path) == record['sha256'],
                'Local asset missing or changed: ' + name)
    old.validate_sm64(old.private_path(target, 'save/baserom.us.z64'))
    assets = target / folder
    if character == 'octane':
        model = read_json(old.private_path(assets, 'model.json'), 65536)
        require(isinstance(model, dict) and model.get('schema') == 'octane-host-mesh-v1' and
                model.get('body_sha256') == 'bedf7fc0d64ab2c6d4f2620a946bb88e600f779c3e924fb80ab96c431d67f7e6' and
                model.get('wheel_sha256') == '9b2582f69e6bf2fd06272b9b545dfd33cfc931f31d373b63a1078a747d902560', 'Unsupported Octane export')
        hashes = ('e1162f1643ad34857fda1284b5ecac7af7d7172069c968b2eb9473b9fdb523df', '46adadb342a27e3b39309087c051d68efcccf7e6cfccf8341a7866062720ff28')
        require(isinstance(model.get('parts'), list) and len(model['parts']) == 2, 'Incomplete Octane model')
        for part, name, digest in zip(model['parts'], ('body', 'wheel'), hashes):
            require(isinstance(part, dict) and part['name'] == name and part['file'] == name + '.bin' and part['sha256'] == digest and
                    old.sha256(old.private_path(assets, part['file'])) == digest, 'Octane original geometry mismatch')
    else:
        model = read_json(old.private_path(assets, 'thps_model.json'), 16 * 1024 * 1024)
        require(isinstance(model, dict) and isinstance(model.get('source'), dict) and
                model['source'].get('rom_sha256') == '506961e65197aaff2b47ae055b7d28470f80fb925914ebeac9ddc541a7a9fd8d' and
                isinstance(model.get('original_pose_execution'), dict) and isinstance(model.get('animations'), list) and
                len(model['animations']) == 78, 'Original Tony model/poses required')
        for name, digest in (('boot.bin', 'e9e91933301ad4f74750ca5c9082490af1cb27a5d2c76371fe1e100721a84395'),
                             ('air_spin_rotations.bin', '2722d314cd18275cfa0bd3f7568782f740818b51bae4beeab1adb382955b9e64')):
            require(old.sha256(old.private_path(assets, name)) == digest, 'Tony original source table mismatch')
    return target


def check_assets(character, root=ROOT):
    target = profile(character, root)
    return old.check_asset_directory(target, character) if character in old.CHARACTERS else check_extra(target, character)


def copy_assets(source, destination, character=None):
    source = Path(source)
    require(source.is_dir() and not old.is_redirect(source), 'Select an ordinary local exported asset folder')
    total = 0
    for path in source.rglob('*'):
        require(not old.is_redirect(path), 'Asset imports cannot contain links/junctions')
        if path.is_file():
            total += path.stat().st_size
            require(total <= 512 * 1024 * 1024, 'Asset import exceeds 512 MiB limit')
            # The Tony exporter names its decoded skeleton hawk.psx.n64.
            # Permit only that exact, verified extracted blob, never a ROM.
            tony_shell = (character == 'tony' and path.relative_to(source).as_posix() == 'hawk.psx.n64'
                          and path.stat().st_size == 184336 and old.sha256(path) ==
                          'b50d3975e2e548ebc4da45af6fc654a2dd9c2013463fb42c4fdddd21833040a4')
            require(tony_shell or path.suffix.lower() not in ('.z64', '.v64', '.n64', '.exe', '.dll', '.upk', '.zip'), 'Select the converted runtime asset folder, not a game/tool folder')
    shutil.copytree(source, destination)


def cancel_check():
    if CANCEL_FILE and CANCEL_FILE.exists():
        raise InterruptedError('Setup canceled. Completed profiles and saves are preserved; retry when ready.')


def run_worker(command, root):
    import time
    import threading
    from collections import deque
    cancel_check()
    tail = deque(maxlen=20)
    process = subprocess.Popen(command, cwd=root, shell=False, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding='utf-8', errors='replace', creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    def read_output():
        for line in process.stdout:
            tail.append(line.rstrip())
            print(line, end='', flush=True)
    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    try:
        while process.poll() is None:
            cancel_check()
            time.sleep(0.1)
        reader.join(timeout=2)
        require(process.returncode == 0, 'Asset extraction failed (exit %s). Check the requested game version and retry.\n%s' % (process.returncode, '\n'.join(tail)))
    finally:
        if process.poll() is None:
            if os.name == 'nt':
                # Only this still-running extraction child and its descendants.
                # Never select a process by name, never terminate the game.
                killer = Path(os.environ['SYSTEMROOT']) / 'System32/taskkill.exe'
                result = subprocess.run([str(killer), '/PID', str(process.pid), '/T', '/F'],
                    shell=False, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
                if result.returncode and process.poll() is None:
                    raise old.SetupError('Could not stop the extraction worker. Wait for it to finish before retrying.')
            else:
                process.terminate()
            process.wait(timeout=30)


def prepare_car_audio(args, target, root=ROOT):
    if (args.character or 'octane') != 'octane':
        return
    import rocket_audio_setup
    directory = old.private_path(target, 'octane-model/audio')
    cache = old.private_path(DATA_ROOT or root, '.runtime/tools/vgmstream-r2117')
    try:
        print('Checking local car sounds...', flush=True)
        print(rocket_audio_setup.prepare(args.game, directory, cache, cancel_check), flush=True)
    except InterruptedError:
        raise
    except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print('Car sounds unavailable: ' + str(error) +
              ' Gameplay assets are ready. Select Rocket League in Setup to retry audio; existing sounds are preserved.', flush=True)


def setup(args, root=ROOT):
    character = args.character or 'octane'
    cancel_check()
    target = profile(character, root)
    repairing = False
    if target.exists():
        try:
            check_assets(character, root)
        except (ValueError, OSError, KeyError, TypeError) as error:
            repairing = True
            print('Existing ' + character + ' assets need repair: ' + str(error), flush=True)
        else:
            prepare_engine(root, target)
            prepare_car_audio(args, target, root)
            print('Existing ' + character + ' assets and shared game data verified. Saves and controls preserved.')
            return
    source = args.sm64
    if not source:
        for choice in ('octane', 'mario', 'link', 'bomberman', 'banjo', 'spiderman', 'tony'):
            fallback = profile(choice, root) / 'save/baserom.us.z64'
            if fallback.is_file():
                source = fallback
                break
    require(source, 'Choose your original Super Mario 64 US ROM (.z64/.v64/.n64 or single-ROM ZIP).')
    rom = sm64_bytes(source)
    if character == 'octane' and not args.assets:
        require(args.game and args.game.is_dir(), 'Choose your Rocket League installation folder containing TAGame. See the supported-source list for accepted package versions.')
        from codex.rocketleague.tools.export_octane import check_game
        check_game(args.game)  # reject missing/unsupported packages before provisioning tools
    if character not in ('mario', 'octane') and not args.assets:
        require(args.rom and args.rom.is_file(), 'Choose the supported original ' + character + ' ROM; see the source-game list.')
    runtime = old.private_path(DATA_ROOT or root, '.runtime')
    runtime.mkdir(parents=True, exist_ok=True)
    if repairing:
        require(not any(runtime.glob('**/.launch-lock')), 'Close the running game before repairing its assets.')
    lock = old.private_path(DATA_ROOT or root, '.runtime/.seven-setup-lock')
    try:
        lock.mkdir()
    except FileExistsError as error:
        raise old.SetupError('Another setup may be running. Close it first. After a forced shutdown, verify no setup is active and remove only this empty lock directory to retry: ' + str(lock)) from error
    try:
        with tempfile.TemporaryDirectory(prefix='.seven-setup-', dir=runtime) as temporary:
            stage = Path(temporary) / 'profile'
            stage.mkdir()
            (stage / 'save').mkdir()
            (stage / 'save/baserom.us.z64').write_bytes(rom)
            if character in ASSETS and args.assets:
                copy_assets(args.assets, stage / ASSETS[character][0], character)
            elif character != 'mario':
                viewer = None
                if character == 'octane':
                    viewer = old.private_path(DATA_ROOT or root, '.runtime/tools/ueviewer-a0bfb468')
                    from download_ueviewer import download
                    print('Checking the pinned official UE Viewer extraction tool...', flush=True)
                    download(viewer, consent=True, cancel_check=cancel_check)
                    cancel_check()
                command = [sys.executable, '-B', '-u', '-X', 'utf8', str(root / 'codex/windows/extract_character.py'),
                           '--character', character, '--output', str(stage), '--scratch', str(Path(temporary))]
                if viewer:
                    command += ['--game', str(args.game), '--ueviewer', str(viewer)]
                else:
                    command += ['--rom', str(args.rom)]
                run_worker(command, root)
            cancel_check()
            old.write_setup_inventory(stage, character)
            if character in old.CHARACTERS:
                old.check_asset_directory(stage, character)
            else:
                check_extra(stage, character)
            cancel_check()
            if repairing:
                # Keep the complete original profile as a recovery copy and
                # copy mutable save/control files byte-for-byte into the new one.
                saved = old.private_path(target, 'save')
                if saved.exists():
                    for path in saved.rglob('*'):
                        require(not old.is_redirect(path), 'Save files must not contain links or junctions')
                        if path.is_file() and path.name != 'baserom.us.z64':
                            destination = stage / 'save' / path.relative_to(saved)
                            destination.parent.mkdir(parents=True, exist_ok=True)
                            shutil.copyfile(path, destination)
                cancel_check()
                backup = target.with_name(target.name + '.before-repair-' + uuid.uuid4().hex)
                target.rename(backup)
                try:
                    stage.rename(target)
                except BaseException:
                    backup.rename(target)
                    raise
                print('Previous profile preserved at ' + str(backup), flush=True)
            else:
                require(not target.exists(), 'Setup appeared; refusing overwrite')
                stage.rename(target)
    finally:
        lock.rmdir()
    prepare_engine(root, target)
    prepare_car_audio(args, target, root)
    print('Setup complete: ' + character + ' assets extracted and verified. Ready to play. Original inputs unchanged.')


def prepare_engine(root, target):
    print('Preparing and verifying shared game data from your saved SM64 input...', flush=True)
    result = engine_setup.prepare(root, DATA_ROOT or root, target / 'save/baserom.us.z64', cancel_check)
    print('Shared game data ' + result['status'] + ' and verified.', flush=True)
    return result


def verify_package(root=ROOT):
    old.check_package(root)
    manifest = read_json(root / 'PACKAGE-MANIFEST.json')
    require(manifest.get('edition') == 'super-rocket-64' and manifest.get('schema_version') == 4, 'Wrong release edition or manifest format')
    for name, record in manifest['files'].items():
        path = old.private_path(root, name)
        require(path.is_file() and path.stat().st_size == record['size'] and old.sha256(path) == record['sha256'], 'Package file missing or changed: ' + name)


def available(root=ROOT, strict=False):
    enabled = {}
    errors = []
    for character in CHARACTERS:
        try:
            if profile(character, root).exists():
                enabled[character] = check_assets(character, root)
        except (ValueError, OSError, KeyError, TypeError) as error:
            errors.append(character + ': ' + str(error))
    if 'mario' not in enabled and 'octane' in enabled:
        enabled['mario'] = enabled['octane']
    if errors:
        message = 'Unavailable local character data: ' + '; '.join(errors)
        require(not strict, message)
        print(message, file=sys.stderr)
    return enabled


def build_command(args, root=ROOT):
    enabled = available(root)
    mode = args.mode
    character = args.character or 'octane'
    require(enabled, 'Use Setup in the launcher first. Only SM64 is required for Mario offline.')
    if mode in ('host', 'join'):
        require(character in ('mario', 'octane'), 'Only Mario/Octane are supported online')
        require('octane' in enabled, 'Every online peer needs Octane setup, including Mario players, to display remote cars')
        require(1024 <= args.port <= 65535, 'Use a port from 1024 to 65535')
        save = old.private_path(DATA_ROOT or root, '.runtime/online-' + mode + '-' + character + '/save')
        asset = enabled['octane'] / 'octane-model'
        options = ['--character-net', str(asset), '--character-wheel']
        if character == 'octane':
            options += ['--rocket-car', str(asset)]
        if mode == 'host':
            options += ['--server', str(args.port)]
            if not args.listen_on_network:
                options += ['--loopback-only']
        else:
            require(args.host and not args.host.startswith('-') and re.fullmatch(r'[A-Za-z0-9.:%_-]{1,253}', args.host), 'Enter the reachable host address, without a URL or spaces')
            options += ['--client', args.host, str(args.port)]
            if args.host == '::1':
                options += ['--loopback-only']
        if args.name:
            require(len(args.name) <= 30 and args.name.isprintable(), 'Player name must be 1-30 printable characters')
            options += ['--playername', args.name]
        source_save = enabled['octane'] / 'save'
    elif mode == 'wheel':
        source_save = enabled.get('mario', next(iter(enabled.values()))) / 'save'
        save = old.private_path(DATA_ROOT or root, '.runtime/combined/save')
        options = ['--offline', '--character-wheel']
        for choice in CHARACTERS:
            if choice in ASSETS and choice in enabled:
                folder, flag = ASSETS[choice]
                options += [flag, str(enabled[choice] / folder)]
    else:
        require(character in enabled, 'Use Setup for ' + character + ' first')
        source_save = enabled[character] / 'save'
        save = source_save
        options = ['--offline']
        if character in ASSETS:
            folder, flag = ASSETS[character]
            options += [flag, str(enabled[character] / folder)]
    command = [str(root / 'sm64coopdx.exe'), '--disable-mods', '--skip-intro', '--skip-update-check', '--no-discord',
               '--hide-loading-screen', '--windowed', '--backend', 'opengl', '--savepath', str(save)] + options
    return command, save, source_save, enabled


def launch(args, root=ROOT):
    require(os.name == 'nt', 'Play in native Windows')
    old.check_location(root)
    verify_package(root)
    require(not old.private_path(DATA_ROOT or root, '.runtime/.seven-setup-lock').exists(), 'Setup is still running')
    require(not old.private_path(DATA_ROOT or root, '.runtime/.engine-setup-lock').exists(), 'Shared-data setup is still running')
    engine_directory = engine_setup.require_ready(root, DATA_ROOT or root)
    command, save, source, enabled = build_command(args, root)
    environment = old.clean_environment(os.environ)
    environment['SUPER_ROCKET64_ENGINE_ASSETS'] = str(engine_directory)
    muted = getattr(args, 'mute', False)
    if muted:
        # Silence this run only; never replace the user's saved volume choices.
        environment['SDL_AUDIODRIVER'] = 'dummy'
    print('Available offline characters: ' + ', '.join(enabled))
    if args.dry_run:
        print(json.dumps(command, indent=2))
        return 0
    save.mkdir(parents=True, exist_ok=True)
    lock = old.private_path(save, '.launch-lock')
    try:
        lock.mkdir()
    except FileExistsError as error:
        raise old.SetupError('This save may be in use. Close its game first; after a crash see README for stale-lock recovery: ' + str(lock)) from error
    try:
        # Check both leaves before any file writes, including an initial ROM copy.
        rom = old.private_path(save, 'baserom.us.z64')
        config = old.private_path(save, 'sm64config.txt')
        if not rom.exists():
            shutil.copyfile(old.private_path(source, 'baserom.us.z64'), rom)
        old.validate_sm64(rom)
        lines = config.read_text().splitlines() if config.exists() else ['window_w 1280', 'window_h 720', 'fullscreen false', 'vsync true', 'framerate_mode 1', 'frame_limit 30', 'interpolation_mode 1', 'bettercam_enable true', 'bettercam_analog true', 'background_gamepad 0']
        if not config.exists():
            config.write_text('\n'.join(lines) + '\n')
        print('Starting ' + ('muted game' if muted else 'game with your saved audio settings') + '. Close the game normally to release this save.')
        return subprocess.call(command, cwd=root, env=environment, shell=False)
    finally:
        lock.rmdir()


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('setup', 'check', 'play', 'status'))
    p.add_argument('--character', choices=CHARACTERS)
    for name in ('sm64', 'rom', 'assets', 'game', 'ueviewer'):
        p.add_argument('--' + name, type=Path)
    p.add_argument('--mode', choices=('wheel', 'single', 'host', 'join'), default='wheel')
    p.add_argument('--host')
    p.add_argument('--port', type=int, default=7777)
    p.add_argument('--name')
    p.add_argument('--listen-on-network', action='store_true')
    p.add_argument('--download-ueviewer', action='store_true', help='Explicitly download the hash-pinned public extraction tool, never game assets')
    p.add_argument('--dry-run', action='store_true')
    p.add_argument('--mute', action='store_true', help='Silence this run without changing saved volume settings')
    p.add_argument('--data-dir', type=Path)
    p.add_argument('--cancel-file', type=Path)
    a = p.parse_args(argv)
    global DATA_ROOT, CANCEL_FILE
    DATA_ROOT = a.data_dir.absolute() if a.data_dir else None
    CANCEL_FILE = a.cancel_file
    try:
        if DATA_ROOT:
            # Reject redirected ancestors before resolving the data root.
            for ancestor in (DATA_ROOT, *DATA_ROOT.parents):
                if ancestor.exists() or ancestor.is_symlink():
                    require(not old.is_redirect(ancestor), 'Choose a data folder without symlinks or junctions')
            old.check_location(DATA_ROOT)
            old.private_path(DATA_ROOT, '.runtime')
        require(sys.version_info >= (3, 10), 'Python 3.10 or newer is required')
        if a.action == 'status':
            errors = {}
            for character in CHARACTERS:
                if profile(character).exists():
                    try:
                        check_assets(character)
                    except (ValueError, OSError, KeyError, TypeError) as error:
                        errors[character] = str(error)
            enabled = available()
            if enabled:
                try:
                    engine_setup.require_ready(ROOT, DATA_ROOT or ROOT)
                except (ValueError, OSError, KeyError, TypeError) as error:
                    errors['shared_game_data'] = str(error)
                    enabled = {}
            print(json.dumps({'ready': list(enabled), 'errors': errors}))
            return 0
        if a.action == 'setup':
            cancel_check()
            verify_package()
            setup(a)
            return 0
        if a.action == 'check':
            verify_package()
            enabled = available(strict=True)
            if enabled:
                engine_setup.require_ready(ROOT, DATA_ROOT or ROOT)
            if a.character:
                require(a.character in enabled, 'That character has not been set up')
            print('Package verified. Available: ' + (', '.join(enabled) or 'none; use Setup in the launcher'))
            print('No game or downloads started. Mario/Octane online also requires local Octane assets on each PC.')
            return 0
        return launch(a)
    except InterruptedError as error:
        print(str(error), file=sys.stderr)
        return 130
    except (ValueError, OSError, KeyError, TypeError, ImportError, subprocess.SubprocessError) as error:
        print('Stopped: ' + str(error), file=sys.stderr)
        return 2
    except (KeyboardInterrupt, EOFError):
        print('Canceled; setup/check never start the game.', file=sys.stderr)
        return 130


if __name__ == '__main__':
    raise SystemExit(main())
