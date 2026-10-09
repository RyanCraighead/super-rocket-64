"""Windowless acceptance against a real Windows engine; no owned assets needed."""
import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

parser = argparse.ArgumentParser()
parser.add_argument('--engine', type=Path, required=True)
parser.add_argument('--launcher-dir', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--old-config', type=Path)
parser.add_argument('--report', type=Path)
args = parser.parse_args()
sys.path.insert(0, str(args.launcher_dir))
import controls_setup as controls
import seven_launcher as launcher
controls_prepare = controls.prepare

checks = 0
probes = 0
def check(value, message):
    global checks
    checks += 1
    assert value, message

def engine(*arguments, expected=0):
    global probes
    probes += 1
    result = subprocess.run([str(args.engine), *map(str, arguments)], capture_output=True, timeout=30,
                            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    check(result.returncode == expected, (arguments, result.returncode, result.stderr.decode(errors='replace')))
    return result.stdout.decode('utf-8')

def loaded(save, shared, edits=None):
    save.mkdir(parents=True, exist_ok=True)
    out = engine('--verify-controls', save, shared, *([] if edits is None else [edits]))
    body = out.split('CONTROLS_BEGIN', 1)[1].split('CONTROLS_END', 1)[0]
    return controls.records(body.encode(), defaults, strict=True), out

def expect_error(action):
    try: action()
    except controls.old.SetupError: pass
    else: raise AssertionError('Expected actionable setup failure')

def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')

defaults = controls.defaults_from_engine(args.engine)
for invalid in (['2', *(['9'*5000]*9), '0', '0', '0'], ['2', *(['1']*9), '2', '0', '0']):
    check(controls.binding_values(invalid) is None, 'Malformed binding record was accepted')
legacy_defaults = {key: list(values) for key, values in defaults.items()}
legacy_defaults['rocket-bindings:'] = ['1', *defaults['rocket-bindings:'][1:9], *defaults['rocket-bindings:'][10:]]
old_custom = ('key_a 002c 1003 1103\nkey_b 002d 1001 1101\nkey_cleft 002e ffff ffff\n'
              'key_cright 002f ffff ffff\nstick_invert_right_x true\nbettercam_inverty false\n'
              'rocket_camera_mode 0\nrocket-bindings: 1 14 13 4 6 5 5 0 0 1 1 1\n')
if args.old_config:
    old_custom = args.old_config.read_text()

with tempfile.TemporaryDirectory(prefix='sr64-controls-') as temporary:
    base = Path(temporary)
    for layout in ('default-layout', 'portable folder'):
        install = base / layout
        root = install / 'new-version'
        root.mkdir(parents=True)
        data = install / 'data'
        launcher.DATA_ROOT = data
        saves = launcher.control_save_directories(root)
        offline, host_mario, host_octane, join_mario, join_octane = saves[:5]
        source = host_octane / 'sm64config.txt'
        write(source, old_custom + 'player_name private-fixture\nnetwork_password private-fixture\n')
        os.utime(source, ns=(1000000000, 1000000000))
        write(offline / 'sm64config.txt', ''.join(k+' '+' '.join(v)+'\n' for k,v in legacy_defaults.items()))
        os.utime(offline / 'sm64config.txt', ns=(2000000000, 2000000000))
        write(join_mario/'sm64config.txt', 'key_a 0026 1000 1110\nkey_cup 0030\n')
        snapshots = {p: p.read_bytes() for p in data.rglob('*') if p.is_file()}
        with controls.locked(data, saves, offline):
            shared = controls.prepare(data, args.engine, saves, offline)
            expect_error(lambda: controls.locked(data, saves, offline).__enter__())
        check(all(p.read_bytes()==raw for p,raw in snapshots.items()), 'Migration changed legacy source')
        check(b'private-fixture' not in shared.read_bytes(), 'Unrelated settings leaked into controls')
        migration = json.loads((shared.parent/'migration.json').read_text())
        check(len(migration['sources'])==3, 'Missing migration backups')
        for record in migration['sources']:
            check((shared.parent/'legacy-backups'/record['backup']).read_bytes()==Path(record['source']).read_bytes(), 'Legacy backup mismatch')
        save_game = offline/'fixture.sav'
        save_game.write_bytes(b'progress\0\xff')
        for save in saves[:5]:
            values, output = loaded(save, shared)
            check(values['key_a']==['002c','1003','1110'], 'Keyboard/gamepad/mouse slot merge')
            check(values['key_b'][0]=='002d' and values['key_cleft'][0]=='002e' and values['key_cright'][0]=='002f', 'Native jump/boost/camera')
            check(values['key_cup']==['0030','ffff','ffff'], 'Missing native slots filled')
            check(values['rocket-bindings:']==['2','14','13','4','6','5','5','0','0','0','1','1','1'], 'Car mapping migration without a new shared Triangle action')
            check('CONTROL_BUTTON 3 1 0 0' in output and 'CONTROL_BUTTON 10 0 1 0' in output and 'CONTROL_BUTTON 9 0 0 1' in output, 'Actual button mapping')
            check('CONTROL_BUTTON 0 0 0 0' in output and 'CONTROL_BUTTON 1 0 0 0' in output, 'Old car buttons remained active')
            check('CONTROL_AXES -12000 15000' in output, 'Actual remapped/inverted stick')
            check(values['rocket_camera_mode']==['0'] and values['bettercam_inverty']==['false'], 'Camera choices')
        check(save_game.read_bytes()==b'progress\0\xff', 'Progression modified')

        # Actual production launcher pipeline; replace only asset checks and final
        # interactive game subprocess in this asset-free fixture. The selected
        # save path/env are fed into the actual engine config loader below.
        enabled = {c: launcher.profile(c, root) for c in ('mario','octane')}
        for p in enabled.values(): write(p/'save/baserom.us.z64', 'inert test input')
        commands = []
        def launch_probe(command, cwd, env, shell):
            check(Path(env['SUPER_ROCKET64_CONTROLS'])==shared, 'Wrong shared path in launch environment')
            save = Path(command[command.index('--savepath')+1])
            check(save in saves, 'Untracked launch save path')
            check(Path(cwd)==root and not shell, 'Changed process invocation')
            values, output = loaded(save, shared)
            check(values['key_a']==['002c','1003','1110'], 'Launcher-loaded keys reset')
            check('CONTROL_BUTTON 10 0 1 0' in output, 'Launcher-loaded boost reset')
            commands.append(command)
            return 0
        with patch.object(launcher, 'available', return_value=enabled), \
             patch.object(launcher, 'verify_package'), patch.object(launcher.old, 'check_location'), \
             patch.object(launcher.old, 'validate_sm64'), patch.object(launcher.engine_setup, 'require_ready', return_value=root), \
             patch.object(launcher.subprocess, 'call', side_effect=launch_probe), \
             patch.object(launcher.controls_setup, 'prepare', side_effect=lambda d,e,s,a: controls_prepare(d,args.engine,s,a)):
            for mode, character in [('wheel','octane'),('single','octane'),('single','mario'),('host','octane'),('host','mario'),('join','octane'),('join','mario')]:
                options = SimpleNamespace(mode=mode,character=character,port=7777,host='127.0.0.1',name=None,listen_on_network=False,mute=False,dry_run=False)
                with contextlib.redirect_stdout(io.StringIO()): check(launcher.launch(options,root)==0, 'Launch pipeline failed')
        check(len(commands)==7, 'Modes omitted')
        check(not list(data.rglob('.launch-lock')), 'Lock leaked')

        # Real save edit path then reload in another mode/version. Explicitly
        # resetting a custom key to its default must not reimport old custom keys.
        edit = base/'edits.cfg'
        write(edit, 'key_a 0026 1000 1103\nkey_b 0034 1002 1101\nkey_cleft 0035 ffff ffff\nrocket_camera_mode 1\nrocket-bindings: 1 14 13 1 2 3 3 0 0 0 0 0\n')
        shared.write_bytes(shared.read_bytes()+b'future_control 42\n')
        previous = shared.read_bytes()
        values, output = loaded(offline,shared,edit)
        check((shared.parent/'controls.cfg.backup').read_bytes()==previous, 'Atomic previous-controls backup')
        check(b'future_control 42' in shared.read_bytes(), 'Future setting lost')
        check(controls.prepare(data,args.engine,saves,join_octane)==shared, 'Shared path changed')
        values, output = loaded(join_octane,shared)
        check(values['key_a']==defaults['key_a'] and values['key_b'][0]=='0034' and values['key_cleft'][0]=='0035', 'Saved edit/reset reimported stale values')
        check('CONTROL_BUTTON 0 1 0 0' in output and 'CONTROL_BUTTON 1 0 1 0' in output, 'Saved reset not applied to input')
        launcher.DATA_ROOT = data
        check(launcher.control_save_directories(install/'next-version')==saves, 'Version changed controls/profile location')
        good = shared.read_bytes()
        for camera in (0,4,6,13,14):
            write(edit, f'rocket-bindings: 2 14 13 4 6 5 5 0 0 {camera} 1 1 1\n')
            values, output = loaded(offline,shared,edit)
            check(values['rocket-bindings:'][9]==str(camera), 'Explicit camera binding was not saved')
            check(values['rocket-bindings:'][1:9]==['14','13','4','6','5','5','0','0'], 'Camera edit changed driving bindings')
            values, output = loaded(join_octane,shared)
            check(values['rocket-bindings:'][9]==str(camera), 'Other play mode lost camera binding')
            controls.prepare(data,args.engine,saves,join_octane)
        shared.write_bytes(good)
        for bad in (b'key_a nope\n', b'key_a 10000\n', b'rocket-bindings: 1 99\n', b'rocket_camera_mode 2\n', b'key_a 0033\0\n', b'x'*65537):
            shared.write_bytes(bad)
            expect_error(lambda: controls.prepare(data,args.engine,saves,offline))
            check(shared.read_bytes()==bad, 'Invalid settings overwritten')
        shared.write_bytes(good)
        with controls.locked(data,saves,offline): pass
        lock=host_mario/'.launch-lock';lock.mkdir()
        expect_error(lambda: controls.locked(data,saves,offline).__enter__())
        lock.rmdir()
        # Failure before final migration commit is retryable, sources unchanged.
        fresh=base/(layout+' retry');legacy=fresh/'old';write(legacy/'sm64config.txt',old_custom)
        raw=(legacy/'sm64config.txt').read_bytes()
        with patch.object(controls.os, 'replace', side_effect=OSError('injected disk failure')):
            try: controls.prepare(fresh,args.engine,[legacy],legacy)
            except OSError: pass
            else: raise AssertionError('Injected failure ignored')
        check(not (fresh/'.runtime/controls/controls.cfg').exists(), 'Partial import became authoritative')
        recovered=controls.prepare(fresh,args.engine,[legacy],legacy)
        check(recovered.is_file() and (legacy/'sm64config.txt').read_bytes()==raw, 'Retry lost source')

        # A new profile's deliberate Triangle sharing beats synthesized Unbound
        # from an older profile, while all eight old driving choices survive.
        mixed=base/(layout+' camera migration');old_save=mixed/'old';new_save=mixed/'new'
        write(old_save/'sm64config.txt',old_custom)
        write(new_save/'sm64config.txt','rocket-bindings: 2 14 13 4 6 5 5 0 0 4 1 1 1\n')
        mixed_shared=controls.prepare(mixed,args.engine,[old_save,new_save],new_save)
        values,output=loaded(new_save,mixed_shared)
        check(values['rocket-bindings:']==['2','14','13','4','6','5','5','0','0','4','1','1','1'], 'Explicit new camera choice replaced by synthesized legacy value')

report={'passed':True,'checks':checks,'actual_engine_probes':probes,'layouts':['persistent data beside versioned engine','portable folder with spaces'],
        'old_config_from_previous_serializer':bool(args.old_config),'launch_modes':7,'assets_mocked_for_public_fixture':True,
        'game_started':False,'physical_controller_tested':False}
if args.report: args.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
