"""Read-only installer inspection. No downloads, extraction, or game launch."""
import contextlib
import io
import json
import os
from pathlib import Path
import re
import shutil


def discover():
    candidates = []
    manifests = Path(os.environ.get('PROGRAMDATA', r'C:\ProgramData')) / 'Epic/EpicGamesLauncher/Data/Manifests'
    if manifests.is_dir():
        for file in list(manifests.glob('*.item'))[:256]:
            try:
                if file.stat().st_size > 65536:
                    continue
                item = json.loads(file.read_text(encoding='utf-8-sig'))
                if 'rocket league' in str(item.get('DisplayName', '')).lower():
                    candidates.append(str(item.get('InstallLocation', '')))
            except (OSError, ValueError, TypeError):
                pass
    steam = []
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam') as key:
            steam.append(Path(winreg.QueryValueEx(key, 'SteamPath')[0]))
    except (ImportError, OSError):
        pass
    steam.append(Path(os.environ.get('PROGRAMFILES(X86)', r'C:\Program Files (x86)')) / 'Steam')
    libraries = list(steam)
    for base in steam:
        try:
            file = base / 'steamapps/libraryfolders.vdf'
            if file.stat().st_size <= 1048576:
                libraries.extend(Path(p.replace('\\\\', '\\')) for p in re.findall(r'"path"\s*"([^"\r\n]+)"', file.read_text(encoding='utf-8')))
        except (OSError, UnicodeError):
            pass
    for library in libraries:
        try:
            file = library / 'steamapps/appmanifest_252950.acf'
            if file.stat().st_size <= 65536:
                match = re.search(r'"installdir"\s*"([^"/\\\r\n]+)"', file.read_text(encoding='utf-8'))
                if match:
                    candidates.append(str(library / 'steamapps/common' / match[1]))
        except (OSError, UnicodeError):
            pass
    return list(dict.fromkeys(p for p in candidates if p and (Path(p) / 'TAGame/CookedPCConsole').is_dir()))


def status(api):
    ready, errors = [], {}
    for character in api.CHARACTERS:
        if api.profile(character).exists():
            try:
                api.check_assets(character)
                ready.append(character)
            except (ValueError, OSError, KeyError, TypeError) as error:
                errors[character] = str(error)
    engine = False
    if ready:
        try:
            api.engine_setup.require_ready(api.ROOT, api.DATA_ROOT or api.ROOT)
            engine = True
        except (ValueError, OSError, KeyError, TypeError) as error:
            errors['engine'] = str(error)
    audio = False
    if 'octane' in ready:
        try:
            import rocket_audio_setup
            rocket_audio_setup.validate(api.profile('octane') / 'octane-model/audio')
            audio = True
        except (ValueError, OSError, KeyError, TypeError):
            pass
    sm64 = False
    for character in api.CHARACTERS:
        source = api.profile(character) / 'save/baserom.us.z64'
        if source.is_file():
            try:
                api.old.validate_sm64(source)
                sm64 = True
                break
            except (ValueError, OSError):
                pass
    return dict(sm64=sm64, ready=ready, errors=errors, engine=engine, playable=engine and 'octane' in ready,
                audio=audio, detected=discover())


def preflight(args, api):
    # Validators are exactly the ones used by extraction; selection is not proof.
    current = status(api)
    character = args.character or 'octane'
    if args.sm64:
        api.sm64_bytes(args.sm64)
    elif not current['sm64']:
        raise ValueError('Choose your original SM64 US ROM: .z64, .v64, .n64, or a ZIP containing only that ROM.')
    if character == 'octane':
        if args.game and character not in current['ready']:
            from codex.rocketleague.tools.export_octane import check_game
            check_game(args.game)
        elif args.game:
            # A verified cached mesh needs no wheel re-extraction. Optional
            # audio/material profiles perform their own source/hash checks.
            if not (Path(args.game) / 'TAGame/CookedPCConsole').is_dir():
                raise ValueError('Choose the Rocket League installation folder containing TAGame (Epic or Steam).')
        elif character not in current['ready']:
            raise ValueError('Choose the Rocket League installation folder containing TAGame (Epic or Steam).')
    elif character not in current['ready'] or args.rom:
        if not args.rom:
            raise ValueError('Choose the supported original ROM for ' + character + ', or unselect this optional character.')
        if character == 'tony':
            from codex.thps.assets.archive import load_rom
            load_rom(args.rom)
        else:
            getattr(api.old, 'validate_' + {'link': 'oot', 'bomberman': 'bm64', 'banjo': 'bk', 'spiderman': 'spiderman'}[character])(args.rom)
    # Reserve accounts for bounded staged assets, source copies and extraction
    # scratch. This is a safety budget, not a claimed exact installed size.
    needed = 768 * 1024 * 1024 if character == 'octane' else 1024 * 1024 * 1024
    parent = api.DATA_ROOT or api.ROOT
    while not parent.exists():
        parent = parent.parent
    free = shutil.disk_usage(parent).free
    if free < needed:
        raise ValueError('Free at least ' + str(needed // 1048576) + ' MB on the installation drive for setup and temporary files.')
    return dict(valid=True, character=character, free_bytes=free, reserve_bytes=needed,
                message=('Existing car geometry verified. Optional sounds and materials will be checked separately.'
                         if character == 'octane' and character in current['ready'] else
                         'Sources verified. Existing completed assets will be checked and reused.'))


def run(args, api):
    try:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = status(api) if args.action == 'wizard-status' else preflight(args, api)
    except (ValueError, OSError, KeyError, TypeError, ImportError) as error:
        result = dict(valid=False, message=str(error))
    print(json.dumps(result))
    return 0
