"""Compatible installed-car migration and explicit revert; never run full setup."""
import contextlib
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
from types import SimpleNamespace

import rocket_material_setup as materials


def verify_engine(api):
    """The release producer declares the tested engine/material pairing."""
    manifest = json.loads(materials.read(api.ROOT / 'PACKAGE-MANIFEST.json', 4 * 1024 * 1024))
    capability = manifest.get('car_materials', {})
    profile_hash = hashlib.sha256(json.dumps(materials.PROFILE, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    engine_hash = manifest['files']['sm64coopdx.exe']['sha256']
    api.require(capability == dict(schema=materials.PROFILE['schema'], profile_sha256=profile_hash, engine_sha256=engine_hash),
                'This launcher package does not declare a compatible car renderer. Update or repair the launcher before retrying; current materials are preserved.')
    materials.verified(api.ROOT / 'sm64coopdx.exe', engine_hash, 64 * 1024 * 1024)


def paths(api):
    target = api.profile('octane')
    active = api.old.private_path(target, 'octane-model/materials')
    saved = api.old.private_path(target, 'octane-model/materials.disabled')
    # Only generated materials with the exact public profile are managed. Do
    # not move unknown extra files, changed materials or redirected descendants.
    for folder in (active, saved):
        if folder.exists():
            materials.validate(folder)
            expected = set(materials.PROFILE['files']) | {'manifest.json'}
            if {p.name for p in folder.iterdir()} != expected:
                raise ValueError('Car materials contain extra files. They were kept unchanged; restore the original generated materials before retrying.')
    api.check_assets('octane')
    materials.verified(active.parent / 'body.bin', materials.PROFILE['body_geometry_sha256'], 3417360)
    if active.exists() and saved.exists():
        raise ValueError('Active and archived car materials both exist. Both were kept unchanged; resolve the duplicate before retrying.')
    return active, saved


def check_source(game):
    if not game:
        raise ValueError('Choose your installed Rocket League folder to add the optional textures.')
    game = Path(game).absolute()
    for ancestor in (game, *game.parents):
        if ancestor.is_symlink() or (ancestor.exists() and getattr(ancestor, 'is_junction', lambda: False)()):
            raise ValueError('Choose a local Rocket League folder without links or junctions.')
    cooked = game / 'TAGame/CookedPCConsole'
    for folder in (game / 'TAGame', cooked):
        if folder.is_symlink() or getattr(folder, 'is_junction', lambda: False)():
            raise ValueError('Choose a local Rocket League folder without links or junctions.')
    try:
        materials.verified(cooked / 'Body_Octane_SF.upk', materials.BODY_SHA, 64 * 1024 * 1024)
        materials.verified(cooked / 'Startup.upk', materials.PROFILE['startup_package_sha256'], 64 * 1024 * 1024)
    except (ValueError, OSError) as error:
        raise ValueError('This Rocket League folder is missing the supported body/Startup packages, or their version differs. Choose another supported local installation. Your current car appearance is unchanged.') from error
    return game


def inspect(args, api):
    result = dict(active=False, reusable=False, can_apply=False, can_undo=False, source='', message='')
    try:
        active, saved = paths(api)
        result.update(active=active.exists(), reusable=saved.exists(), can_apply=not active.exists(), can_undo=active.exists())
        if active.exists():
            result['message'] = 'Improved body textures and dark windows are active. Revert restores the previous plain appearance.'
        elif saved.exists():
            result['message'] = 'You chose the previous appearance. Switch back to reuse your verified textures without a source or download.'
        else:
            import wizard_setup
            candidates = [args.game] if args.game else wizard_setup.discover()[:8]
            for candidate in candidates:
                try:
                    result['source'] = str(check_source(candidate))
                    break
                except (ValueError, OSError):
                    pass
            result['message'] = ('Supported local textures found. Ready to upgrade the car appearance.' if result['source'] else
                                 'Choose your installed Rocket League folder to finish the car appearance update. The game remains playable.')
    except (ValueError, OSError, KeyError, TypeError) as error:
        result['message'] = 'Appearance unavailable: ' + str(error)
    return result


def require_idle(api):
    runtime = api.old.private_path(api.DATA_ROOT or api.ROOT, '.runtime')
    api.require(not any(runtime.glob('**/.launch-lock')), 'Close the game normally before changing car appearance.')
    api.require(not (runtime / '.engine-setup-lock').exists(), 'Wait for the current setup to finish.')
    return runtime


def change(args, api):
    api.cancel_check()
    verify_engine(api)
    runtime = require_idle(api)
    active, saved = paths(api)
    lock = api.old.private_path(api.DATA_ROOT or api.ROOT, '.runtime/.seven-setup-lock')
    # Reuse the same lock that Play/full setup observe. Do not remove somebody
    # else's lock and check again immediately before the atomic publication.
    try:
        lock.mkdir()
    except FileExistsError as error:
        raise ValueError('Another setup is running. Wait for it to finish before changing car appearance.') from error
    try:
        require_idle(api)
        active, saved = paths(api)
        if args.action == 'appearance-undo':
            api.require(active.exists(), 'The car already uses its previous appearance.')
            api.cancel_check()
            require_idle(api)
            verify_engine(api)
            active.rename(saved)
        elif not active.exists():
            if saved.exists():
                api.cancel_check()
                require_idle(api)
                verify_engine(api)
                saved.rename(active)
            else:
                game = check_source(args.game)
                # Export makes only bounded temporary package copies. Check both
                # drives before any tool provisioning or extraction.
                for folder in (runtime, Path(tempfile.gettempdir())):
                    api.require(shutil.disk_usage(folder).free >= 256 * 1024 * 1024,
                                'Free at least 256 MB on the installation and temporary-file drives, then retry.')
                viewer = api.old.private_path(api.DATA_ROOT or api.ROOT, '.runtime/tools/ueviewer-a0bfb468')
                from download_ueviewer import download
                download(viewer, consent=True, cancel_check=api.cancel_check)
                with tempfile.TemporaryDirectory(prefix='.appearance-', dir=active.parent) as temporary:
                    stage = Path(temporary) / 'verified'
                    materials.export(game, viewer, stage, api.cancel_check)
                    materials.validate(stage)
                    api.cancel_check()
                    require_idle(api)
                    verify_engine(api)
                    api.require(not active.exists() and not saved.exists(), 'Car appearance changed during extraction. Retry after the other operation finishes.')
                    stage.rename(active)
    finally:
        lock.rmdir()
    return inspect(args, api)


def migrate(args, api):
    result = inspect(args, api)
    result.update(needs_attention=False, deferred=False)
    # A verified archived sidecar is an explicit Revert choice. Never undo that
    # choice automatically, nor overwrite customized/unknown generated files.
    if result['active'] or result['reusable']:
        return result
    if not result['can_apply'] or not result['source']:
        result['needs_attention'] = True
        return result
    try:
        require_idle(api)
    except ValueError:
        result.update(deferred=True, message='Car appearance will update after the game or setup has closed.')
        return result
    try:
        result = change(SimpleNamespace(action='appearance-apply', game=Path(result['source'])), api)
        result.update(needs_attention=False, deferred=False)
    except InterruptedError:
        raise
    except (ValueError, OSError, KeyError, TypeError, ImportError) as error:
        result.update(needs_attention=True, message='Car appearance update could not finish: ' + str(error))
    return result


def run(args, api):
    # Provisioning/extraction diagnostics remain available in the helper log;
    # stdout is one bounded JSON report for the launcher, never a fake success.
    with contextlib.redirect_stdout(sys.stderr):
        if args.action == 'appearance-status':
            result = inspect(args, api)
        else:
            api.verify_package()
            result = migrate(args, api) if args.action == 'appearance-migrate' else change(args, api)
    print(json.dumps(result))
    return 0
