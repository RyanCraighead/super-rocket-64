"""Opt-in combined/world QA: two fresh muted loopback peers, one SDL driver.

The exit status describes run structure and basic peer convergence. The separate
route_evidence_complete field describes the scenario analyzer's observations;
neither is full feature or WAN acceptance. The outer wrapper archives artifacts.
Use --self-test for headless parser/evidence tests without launching anything.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from test_boss_network_windows import (ROOT, digest, window_for_pid, foreground_pid,
                                      foreground_diagnostic, closed_process_completion, focus)
from combined_acceptance import analyze as analyze_combined
from analyze_world_qa import analyze as analyze_world
from cap_progress_acceptance import analyze as analyze_cap_progress
from coin_native_acceptance import analyze as analyze_coin_followup

COMBINED = ('metal', 'wing', 'vanish', 'incoming', 'bump', 'supersonic')
WORLD = ('bob-platform', 'jrb-water', 'sl-ice')
ROM_SHA1 = '9bef1128717f958171a4afac3ed78ee2bb4e86ce'
CLIENT_PREFERENCES = (0, 1)  # Coin-only / Native, independent of effective host rules.
SPECIAL_CAPS = 0x0e  # MARIO_SPECIAL_CAPS: vanish=2, metal=4, wing=8.
ABORT_REASONS = {'focus_lost', 'unexpected_ui_capture', 'invalid_configuration',
                 'invalid_setup_or_device', 'nondefault_bindings_pending'}


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'rom', 'octane', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--sha256', required=True, help='Required immutable QA executable SHA-256')
    parser.add_argument('--rom-sha1', choices=(ROM_SHA1,), default=ROM_SHA1,
                        help='Pinned hash of the privately supplied supported ROM')
    parser.add_argument('--driver', choices=('host', 'client'), required=True)
    parser.add_argument('--scenario', choices=COMBINED + WORLD, required=True)
    parser.add_argument('--cap-progression', action='store_true', help='Exercise the native Wing switch and box progression route')
    parser.add_argument('--coin-followup', action='store_true', help='Follow the real supersonic Goomba drop through native pickup and credit')
    parser.add_argument('--port', type=int, default=27926)
    parser.add_argument('--boost-mode', choices=('finite', 'infinite'), default='infinite',
                        help='Host rule; client always persists finite')
    parser.add_argument('--surface-mode', choices=('car', 'native'), default='car',
                        help='Host rule; client always persists native')
    parser.add_argument('--timeout', type=float, default=160, help='Overall owned-run bound, 30..220 seconds')
    parser.add_argument('--allow-focused-test', action='store_true')
    return parser


def host_rules(args):
    return int(args.boost_mode == 'infinite'), int(args.surface_mode == 'native')


def launch_plan(role, args, directory, inherited=None):
    """Pure construction: no directory, process, controller or focus side effects."""
    driving = role == args.driver
    boost, surface = host_rules(args) if role == 'host' else CLIENT_PREFERENCES
    save = directory / 'save'
    config = (
        'window_w 1280\nwindow_h 720\nfullscreen false\nvsync false\nframe_limit 30\n'
        'framerate_mode 1\ninterpolation_mode 1\nmaster_volume 0\nmusic_volume 0\n'
        'sfx_volume 0\nenv_volume 0\nbackground_gamepad 0\nbettercam_enable true\n'
        f'bettercam_analog true\ndisable_gamepads {str(not driving).lower()}\n'
        f'rocket_boost_mode {boost}\nrocket_surface_mode {surface}\n')
    env = {k: v for k, v in (os.environ if inherited is None else inherited).items()
           if not k.startswith(('SM64_ROCKET_QA_', 'SM64_CHARACTER_NET_', 'SM64_BOSS_NET_', 'SM64_COMBINED_NET_'))
           and k not in ('THPS_SCORE_SCENARIO', 'SM64_QA_OUTPUT_DIR', 'SM64_ROCKET_PLATFORM_QA')}
    env.update(SDL_AUDIODRIVER='dummy', SM64_CHARACTER_NET_MOTION='1',
               SM64_CHARACTER_NET_OBSERVE=str(directory),
               SM64_CHARACTER_NET_SECONDS=str(min(240, math.ceil(args.timeout) + 20)),
               SM64_COMBINED_NET_OBSERVE='outside' if driving else args.scenario)
    if args.scenario == 'bob-platform':
        env['SM64_ROCKET_PLATFORM_QA'] = '1'
    if args.coin_followup:
        env['SM64_ROCKET_QA_COIN_FOLLOWUP'] = '1'
    if driving:
        env.update(SM64_ROCKET_QA_OUTPUT=str(directory), SM64_ROCKET_QA_GAMEPAD='1')
        env['SM64_ROCKET_QA_WORLD' if args.scenario in WORLD else 'SM64_ROCKET_QA_COMBINED'] = args.scenario
        if args.cap_progression:
            env['SM64_ROCKET_QA_CAP_PROGRESS'] = 'wing'
    command = [str(args.binary), '--console', '--disable-mods', '--hide-loading-screen',
               '--skip-intro', '--skip-update-check', '--no-discord', '--windowed',
               '--savepath', str(save), '--character-net', str(args.octane), '--loopback-only',
               '--rocket-car', str(args.octane), '--playername', 'Combined-' + role, '--backend', 'opengl']
    command += ['--server', str(args.port)] if role == 'host' else ['--client', '::1', str(args.port)]
    return command, env, config


def finite_tree(value):
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, dict):
        return all(finite_tree(v) for v in value.values())
    if isinstance(value, list):
        return all(finite_tree(v) for v in value)
    return True


def records(text, prefix):
    values, errors = [], []
    for number, line in enumerate(text.splitlines(), 1):
        if not line.startswith(prefix + ' '):
            continue
        try:
            value = json.loads(line[len(prefix) + 1:])
            if not isinstance(value, dict) or not finite_tree(value):
                raise ValueError('Expected finite JSON object')
            values.append(value)
        except (ValueError, TypeError) as error:
            errors.append(f'{prefix} line {number}: {error}')
    return values, errors


def end_record(text, scenario):
    prefix = 'ROCKET_CAP_PROGRESS_END' if 'ROCKET_CAP_PROGRESS_BEGIN ' in text else 'ROCKET_WORLD_QA_END' if scenario in WORLD else 'ROCKET_COMBINED_QA_END'
    values, errors = records(text, prefix)
    return values[0] if len(values) == 1 and not errors else None


def expected_shutdown(text, scenario):
    end = end_record(text, scenario)
    return bool(end and end.get('reason') not in ABORT_REASONS)


def same_area_span(rows):
    return sum(b['ms'] - a['ms'] for a, b in zip(rows, rows[1:])
               if 0 < b['ms'] - a['ms'] <= 750)


def network_evidence(logs, target_level, boost, surface):
    checks, observed, errors, parsed = {}, {}, [], {}
    for role in ('host', 'client'):
        parsed[role] = {}
        for label, prefix in (('local', 'COMBINED_NET_LOCAL'), ('remote', 'COMBINED_NET_REMOTE'), ('motion', 'CNET_MOTION')):
            data, problems = records(logs.get(role, ''), prefix)
            parsed[role][label] = data
            errors += [role + ': ' + e for e in problems]
    try:
        for role, peer in (('host', 'client'), ('client', 'host')):
            own_global, peer_global = (0, 1) if role == 'host' else (1, 0)
            local = [r for r in parsed[role]['local'] if r['level'] == target_level]
            remote = [r for r in parsed[role]['remote'] if r['level'] == target_level and
                      r['global'] == own_global and r['peer'] == peer_global and
                      r['valid'] and r['area'] and r['active'] in (1, 2)]
            checks[role + '_identity'] = bool(local) and all(r['global'] == own_global for r in local)
            checks[role + '_same_area_fresh_car'] = len(remote) >= 3 and same_area_span(remote) >= 1000
            checks[role + '_host_rules_converged'] = len(local) >= 3 and all(
                (r['boost_mode'], r['surface_mode']) == (boost, surface) for r in local[-3:])
            sent = {(r['epoch'], r['area_sequence'], r['sequence']): r for r in parsed[peer]['motion']
                    if r['stage'] == 'sent' and r['level'] == target_level and r['index'] == 0 and r['active'] in (1, 2)}
            accepted = [r for r in parsed[role]['motion'] if r['stage'] == 'accepted' and
                        r['level'] == target_level and r['index'] != 0 and r['active'] in (1, 2)]
            matches = [(sent[(r['epoch'], r['area_sequence'], r['sequence'])], r) for r in accepted
                       if (r['epoch'], r['area_sequence'], r['sequence']) in sent]
            position_delta = [math.dist(a['pos'], b['pos']) for a, b in matches]
            velocity_delta = [math.dist(a['vel'], b['vel']) for a, b in matches]
            checks[role + '_accepted_peer_poses_match'] = len(matches) >= 5 and all(
                pd <= 1 and vd <= 2 and abs(a['boost'] - b['boost']) <= .05 and a['active'] == b['active']
                for (a, b), pd, vd in zip(matches, position_delta, velocity_delta))
            observed[role] = dict(local_target_samples=len(local), fresh_peer_samples=len(remote),
                same_area_observed_ms=same_area_span(remote), matched_raw_pose_samples=len(matches),
                maximum_raw_position_delta=max(position_delta, default=None),
                maximum_raw_velocity_delta=max(velocity_delta, default=None),
                remote_draw_calls_observed=bool(remote) and any(r['draws'] > 0 for r in remote),
                remote_activity_states=sorted({r['active'] for r in remote}),
                remote_caps_observed=sorted({r['caps'] & SPECIAL_CAPS for r in remote}),
                remote_visual_special_caps_observed=sorted({r['visual_caps'] & SPECIAL_CAPS
                    for r in remote if 'visual_caps' in r}),
                remote_water_modes_observed=sorted({r['water'] for r in remote}),
                final_observed_rules=[local[-1]['boost_mode'], local[-1]['surface_mode']] if local else None)
    except (KeyError, ValueError, TypeError, IndexError) as error:
        errors.append('Required peer telemetry missing/invalid: ' + str(error))
    checks['peer_telemetry_valid'] = not errors
    return dict(checks=checks, observations=observed, errors=errors)


def screenshots(text, directory, coin_followup=False):
    files, errors = [], []
    for line in text.splitlines():
        match = re.match(r'^ROCKET_QA_CAPTURE (.*?) renderer=', line)
        if not match:
            continue
        image = Path(match[1]).resolve()
        if image.parent != directory.resolve() or not image.name.startswith('frame-') or image.suffix != '.ppm':
            errors.append('Framebuffer path is outside the owned driver directory')
            continue
        try:
            with image.open('rb') as stream:
                valid = stream.read(3) == b'P6\n'
            if not valid:
                raise ValueError('Not a native PPM framebuffer')
            files.append(dict(file=image.name, sha256=digest(image), bytes=image.stat().st_size))
        except (OSError, ValueError) as error:
            errors.append(image.name + ': ' + str(error))
    maximum = 4 if coin_followup else 3
    if not 1 <= len(files) <= maximum or len({f['file'] for f in files}) != len(files):
        errors.append(f'Expected one to {maximum} distinct native driver captures')
    return files, errors


def route_evidence(text, directory, scenario, coin_followup=False):
    try:
        if 'ROCKET_CAP_PROGRESS_BEGIN ' in text:
            report = analyze_cap_progress(text, directory)
            stage, _ = records(text, 'ROCKET_CAP_PROGRESS_STAGE')
            valid = report.get('checks', {}).get('well_formed_evidence', False)
            complete = bool(report.get('passed'))
        elif scenario in COMBINED:
            report = analyze_combined(text, directory)
            stage, _ = records(text, 'ROCKET_COMBINED_QA_STAGE')
            valid = bool(report.get('run_valid')) and report.get('scenario') == scenario
            complete = bool(report.get('route_evidence_complete'))
        else:
            report = analyze_world(text.splitlines())
            stage, _ = records(text, 'ROCKET_WORLD_QA_STAGE')
            structural = ('telemetry_parsed', 'one_normal_entry', 'observations_present', 'scene_consistent',
                          'focus_preserved', 'live_physics_without_reset', 'finite_tank_range')
            valid = not report.get('errors') and report.get('scenario') == scenario and all(
                report.get('checks', {}).get(key) for key in structural)
            complete = bool(report.get('accepted'))
        frames, problems = screenshots(text, directory, coin_followup)
        return dict(analysis=report, route_evidence_complete=complete,
                    structurally_valid=bool(valid and not problems), screenshots=frames,
                    screenshot_errors=problems, target_level=stage[0]['level'] if len(stage) == 1 else None)
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        return dict(analysis_error=str(error), route_evidence_complete=False,
                    structurally_valid=False, target_level=None, screenshots=[])


def persisted_preferences(path):
    values = {}
    if path.is_file():
        for line in path.read_text(errors='replace').splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[0] in ('rocket_boost_mode', 'rocket_surface_mode'):
                try:
                    values[parts[0]] = int(parts[1])
                except ValueError:
                    pass
    return values.get('rocket_boost_mode'), values.get('rocket_surface_mode')


def private_output(path):
    directory = path.resolve()
    runtime = (ROOT / 'codex/.runtime').resolve()
    return directory != runtime and directory.is_relative_to(runtime)


def stopped(output):
    return (output / 'stop-all').exists() or (ROOT.parent / 'STOP-NATIVE-QA').exists()


def finish_process(process):
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if argv == ['--self-test']:
        return self_tests()
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.cap_progression and args.scenario != 'wing':
        parser.error('Native cap progression currently requires --scenario wing')
    if args.coin_followup and (args.scenario != 'supersonic' or args.boost_mode != 'finite'):
        parser.error('Native coin follow-up currently requires --scenario supersonic --boost-mode finite')
    if os.name != 'nt' or not args.allow_focused_test:
        parser.error('An explicitly coordinated Windows foreground test is required')
    if not 1024 <= args.port <= 65535 or not math.isfinite(args.timeout) or not 30 <= args.timeout <= 220:
        parser.error('Port must be 1024..65535 and timeout 30..220 seconds')
    if not re.fullmatch(r'[0-9a-fA-F]{64}', args.sha256):
        parser.error('Supply the exact executable SHA-256')
    args.sha256 = args.sha256.lower()
    for name in ('binary', 'rom', 'octane', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    if not args.binary.is_file() or not args.rom.is_file() or not args.octane.is_dir():
        parser.error('An existing binary, private ROM and private Octane asset directory are required')
    if digest(args.binary) != args.sha256 or digest(args.rom, 'sha1') != args.rom_sha1:
        parser.error('Executable or supported private ROM hash mismatch')
    if not private_output(args.output) or args.output.exists():
        parser.error('Use a fresh child directory of private codex/.runtime')
    if stopped(args.output):
        parser.error('A native QA stop request is present')
    existing = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command',
        'Get-Process sm64coopdx* -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id'],
        capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    if existing.stdout.strip() or existing.returncode not in (0, 1) or existing.stderr.strip():
        parser.error('A native session is running or its absence cannot be verified; preserve it')
    args.output.mkdir(parents=True, exist_ok=False)
    processes, streams, issues = [], [], []
    focused = focus_lost = stop_requested = timed_out = peer_exited = False
    focus_loss_diagnostic = None
    host = client = driver = observer = None

    def launch(role):
        directory = args.output / role
        save = directory / 'save'
        save.mkdir(parents=True, exist_ok=False)
        shutil.copyfile(args.rom, save / 'baserom.us.z64')
        command, env, config = launch_plan(role, args, directory)
        (save / 'sm64config.txt').write_text(config)
        out = (directory / 'stdout.log').open('w')
        err = (directory / 'stderr.log').open('w')
        streams.extend((out, err))
        process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=out, stderr=err,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        entry = dict(role=role, directory=directory, process=process, driver=role == args.driver)
        processes.append(entry)
        print(f'Combined native {role}: PID {process.pid}, driver={entry["driver"]}', flush=True)
        return entry

    try:
        start = time.monotonic()
        host = launch('host')
        for _ in range(30):
            if stopped(args.output):
                stop_requested = True
                raise RuntimeError('Stop requested during host initialization')
            if host['process'].poll() is not None:
                raise RuntimeError('Host exited before client launch')
            time.sleep(.1)
        client = launch('client')
        driver, observer = (host, client) if args.driver == 'host' else (client, host)
        # The observer is intentionally hidden; only wait for the owned driver.
        for _ in range(100):
            if stopped(args.output):
                stop_requested = True
                break
            if driver['process'].poll() is not None or window_for_pid(driver['process'].pid):
                break
            time.sleep(.1)
        if not stop_requested and driver['process'].poll() is None:
            focused = foreground_pid() == driver['process'].pid or focus(driver['process'].pid)
        focus_lost = not focused and not stop_requested
        if focus_lost:
            focus_loss_diagnostic = foreground_diagnostic(driver['process'])
        while not focus_lost and not stop_requested and driver['process'].poll() is None:
            if time.monotonic() - start >= args.timeout:
                timed_out = True
                break
            stop_requested = stopped(args.output)
            if stop_requested:
                break
            if observer['process'].poll() is not None:
                peer_exited = True
                break
            if foreground_pid() != driver['process'].pid:
                # Account only for intentional shutdown's destroyed-window race.
                if closed_process_completion(driver['process'], driver['directory'] / 'stderr.log',
                        lambda text: expected_shutdown(text, args.scenario)):
                    break
                focus_lost = True
                focus_loss_diagnostic = foreground_diagnostic(driver['process'])
                break
            time.sleep(.2)
        if driver['process'].poll() is None:
            finish_process(driver['process'])  # Only an owned process is stopped.
        (observer['directory'] / 'stop').touch()
        try:
            observer['process'].wait(timeout=10)
        except subprocess.TimeoutExpired:
            issues.append('Observer did not honor its local stop marker')
            finish_process(observer['process'])
    except Exception as error:
        issues.append(type(error).__name__ + ': ' + str(error))
    finally:
        for entry in processes:
            try:
                if entry['process'].poll() is None:
                    finish_process(entry['process'])
            except (OSError, subprocess.TimeoutExpired) as error:
                issues.append('Owned process cleanup failed: ' + str(error))
        for stream in streams:
            stream.close()

    logs = {role: (args.output / role / 'stderr.log').read_text(errors='replace')
            if (args.output / role / 'stderr.log').is_file() else '' for role in ('host', 'client')}
    source = logs[args.driver]
    route = route_evidence(source, args.output / args.driver, args.scenario, args.coin_followup)
    coin_report = analyze_coin_followup(logs, args.driver, args.output / args.driver) if args.coin_followup else None
    end = end_record(source, args.scenario)
    if end and end.get('reason') == 'focus_lost':
        focus_lost = True
    boost, surface = host_rules(args)
    network = network_evidence(logs, route['target_level'], boost, surface)
    checks = dict(two_owned_peers_launched=len(processes) == 2,
        native_processes_ok=len(processes) == 2 and all(p['process'].returncode == 0 for p in processes),
        initial_focus=bool(focused), uninterrupted_focus=not focus_lost,
        no_stop_request=not stop_requested, no_runner_timeout=not timed_out,
        observer_survived_driver=not peer_exited, no_runner_errors=not issues,
        driver_shutdown_expected=expected_shutdown(source, args.scenario),
        driver_run_structure_valid=route['structurally_valid'],
        client_preferences_preserved=persisted_preferences(args.output / 'client/save/sm64config.txt') == CLIENT_PREFERENCES)
    checks.update(network['checks'])
    result = dict(schema='octane-combined-native-network-v1', binary_sha256=args.sha256,
        rom_sha1=args.rom_sha1, scenario=args.scenario, driver=args.driver, observer_car=True,
        cap_progression=args.cap_progression, coin_followup=args.coin_followup, coin_evidence=coin_report,
        host_boost_mode=args.boost_mode, host_surface_mode=args.surface_mode,
        client_persisted_preference=dict(boost='finite', surface='native'),
        preference_conflicts_exercised=dict(boost=boost != CLIENT_PREFERENCES[0], surface=surface != CLIENT_PREFERENCES[1]),
        checks=checks, passed=all(checks.values()), route_evidence_complete=route['route_evidence_complete'] and
            (not args.coin_followup or coin_report['passed']),
        full_feature_acceptance=False, driver_gameplay=route, network=network, errors=issues,
        focus_loss_diagnostic=focus_loss_diagnostic,
        processes=[dict(role=p['role'], pid=p['process'].pid, exit_code=p['process'].returncode,
                        directory=p['directory'].name) for p in processes],
        muted=True, loopback_only=True, internet_tested=False, direct_actor_state_writes=False,
        input='owned driver virtual SDL controller only',
        pending=['Human inspection of framebuffer artifacts',
                 'Scenario-specific remote effects/authority, deduplication and reconnect beyond basic pose/rule convergence',
                 'Physical controller/remapped bindings and WAN behavior'])
    (args.output / 'result.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0 if result['passed'] else 1


def self_tests():
    """Small synthetic orchestration/evidence tests; no native evidence claimed."""
    import contextlib
    import io
    import unittest
    from unittest.mock import patch

    def args(scenario='metal', driver='host'):
        return build_parser().parse_args(['--binary', 'qa.exe', '--rom', 'private.z64',
            '--octane', 'private-octane', '--output', 'private-output', '--sha256', 'a'*64,
            '--scenario', scenario, '--driver', driver])

    def peer_logs():
        logs = {}
        for role, global_id, peer_id in (('host', 0, 1), ('client', 1, 0)):
            lines = []
            for i in range(6):
                local = dict(ms=500*i, global_index=global_id, level=9, active=1, boost_mode=1, surface_mode=0)
                local['global'] = local.pop('global_index')
                remote = dict(ms=500*i, peer=peer_id, area=1, level=9, valid=1, active=1,
                              draws=i*10, caps=0, water=0)
                remote['global'] = global_id
                lines += ['COMBINED_NET_LOCAL '+json.dumps(local), 'COMBINED_NET_REMOTE '+json.dumps(remote)]
                for stage, index in (('sent', 0), ('accepted', 1)):
                    motion = dict(stage=stage, index=index, level=9, epoch=1, area_sequence=2,
                                  sequence=i+1, active=1, pos=[i, 2, 3], vel=[1, 2, 3], boost=50)
                    lines.append('CNET_MOTION '+json.dumps(motion))
            logs[role] = '\n'.join(lines)
        return logs

    class RunnerTests(unittest.TestCase):
        def test_all_scenarios_parse(self):
            for scenario in COMBINED+WORLD:
                self.assertEqual(args(scenario).scenario, scenario)

        def test_hash_required(self):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build_parser().parse_args(['--scenario', 'wing'])

        def test_no_permissive_rom_hash(self):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build_parser().parse_args(['--rom-sha1', '0'*40])

        def test_both_peers_are_loopback_cars(self):
            for role in ('host', 'client'):
                command, _, _ = launch_plan(role, args(), Path(role), {})
                self.assertIn('--loopback-only', command)
                self.assertIn('--rocket-car', command)
                if role == 'client':
                    self.assertEqual(command[command.index('--client')+1], '::1')

        def test_only_driver_gets_virtual_input(self):
            for driver in ('host', 'client'):
                for role in ('host', 'client'):
                    _, env, config = launch_plan(role, args(driver=driver), Path(role), {})
                    self.assertEqual('SM64_ROCKET_QA_GAMEPAD' in env, role == driver)
                    self.assertIn('disable_gamepads '+str(role != driver).lower(), config)
                    self.assertEqual(env['SM64_COMBINED_NET_OBSERVE'], 'outside' if role == driver else 'metal')

        def test_coin_followup_is_opt_in_and_passive_on_observer(self):
            value=args('supersonic','client');value.boost_mode='finite';value.coin_followup=True
            for role in ('host','client'):
                _,env,_=launch_plan(role,value,Path(role),{})
                self.assertEqual(env['SM64_ROCKET_QA_COIN_FOLLOWUP'],'1')
                self.assertEqual('SM64_ROCKET_QA_GAMEPAD' in env,role=='client')
            value.coin_followup=False
            _,env,_=launch_plan('host',value,Path('host'),{'SM64_ROCKET_QA_COIN_FOLLOWUP':'1'})
            self.assertNotIn('SM64_ROCKET_QA_COIN_FOLLOWUP',env)

        def test_coin_followup_refuses_other_scenarios_or_infinite(self):
            for scenario,boost in (('wing','finite'),('supersonic','infinite')):
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    main(['--binary','qa.exe','--rom','private.z64','--octane','private-octane',
                          '--output','private-output','--sha256','a'*64,'--scenario',scenario,
                          '--driver','host','--boost-mode',boost,'--coin-followup'])

        def test_world_dispatch_and_stale_environment_cleared(self):
            _, env, _ = launch_plan('client', args('jrb-water', 'client'), Path('client'),
                {'SM64_ROCKET_QA_BOSS': 'bob', 'SM64_BOSS_NET_OBSERVE': 'bowser', 'KEEP': 'yes'})
            self.assertEqual(env['SM64_ROCKET_QA_WORLD'], 'jrb-water')
            self.assertNotIn('SM64_ROCKET_QA_BOSS', env)
            self.assertNotIn('SM64_BOSS_NET_OBSERVE', env)
            self.assertEqual(env['KEEP'], 'yes')

        def test_muted_profiles_and_opposite_defaults(self):
            _, _, host_config = launch_plan('host', args(), Path('host'), {})
            _, _, client_config = launch_plan('client', args(), Path('client'), {})
            for config in (host_config, client_config):
                for key in ('master_volume', 'music_volume', 'sfx_volume', 'env_volume'):
                    self.assertIn(key+' 0\n', config)
            self.assertIn('rocket_boost_mode 1\nrocket_surface_mode 0', host_config)
            self.assertIn('rocket_boost_mode 0\nrocket_surface_mode 1', client_config)

        def test_host_rule_choices_do_not_rewrite_client_preference(self):
            value = args();value.boost_mode='finite';value.surface_mode='native'
            _, _, config = launch_plan('client', value, Path('client'), {})
            self.assertIn('rocket_boost_mode 0\nrocket_surface_mode 1', config)

        def test_output_must_be_private_child(self):
            self.assertTrue(private_output(ROOT/'codex/.runtime/fresh'))
            self.assertFalse(private_output(ROOT/'codex/.runtime'))
            self.assertFalse(private_output(ROOT/'codex/.runtime/../../outside'))

        def test_complete_transport_fixture(self):
            report=network_evidence(peer_logs(), 9, 1, 0)
            self.assertTrue(all(report['checks'].values()), report)

        def test_normal_cap_bits_are_not_power_evidence(self):
            logs=peer_logs()
            logs={role: text.replace('"caps": 0', '"caps": 5, "visual_caps": 17')
                  for role, text in logs.items()}
            report=network_evidence(logs, 9, 1, 0)
            self.assertEqual(report['observations']['host']['remote_caps_observed'], [4])
            self.assertEqual(report['observations']['host']['remote_visual_special_caps_observed'], [0])

        def test_no_same_area_is_not_convergence(self):
            logs=peer_logs();logs['client']=logs['client'].replace('"area": 1', '"area": 0')
            self.assertFalse(network_evidence(logs, 9, 1, 0)['checks']['client_same_area_fresh_car'])

        def test_wrong_rules_are_not_authority(self):
            logs=peer_logs();logs['client']=logs['client'].replace('"boost_mode": 1', '"boost_mode": 0')
            self.assertFalse(network_evidence(logs, 9, 1, 0)['checks']['client_host_rules_converged'])

        def test_positions_must_match_same_accepted_identity(self):
            logs=peer_logs();logs['client']=logs['client'].replace('"pos": [0, 2, 3]', '"pos": [500, 2, 3]')
            self.assertFalse(network_evidence(logs, 9, 1, 0)['checks']['client_accepted_peer_poses_match'])

        def test_wrong_epoch_does_not_match(self):
            logs=peer_logs();logs['client']=logs['client'].replace('"epoch": 1', '"epoch": 5')
            self.assertFalse(network_evidence(logs, 9, 1, 0)['checks']['client_accepted_peer_poses_match'])

        def test_nonfinite_or_missing_peer_data_fails(self):
            logs=peer_logs();logs['client']+='\nCOMBINED_NET_LOCAL {"fuel": NaN}'
            self.assertFalse(network_evidence(logs, 9, 1, 0)['checks']['peer_telemetry_valid'])
            self.assertFalse(all(network_evidence({}, 9, 1, 0)['checks'].values()))

        def test_focus_failure_not_expected_window_shutdown(self):
            self.assertFalse(expected_shutdown('ROCKET_WORLD_QA_END {"reason":"focus_lost"}', 'jrb-water'))
            self.assertTrue(expected_shutdown('ROCKET_WORLD_QA_END {"reason":"route_timeout"}', 'jrb-water'))
            self.assertFalse(expected_shutdown('', 'metal'))

        def test_completion_can_flush_after_window_is_destroyed(self):
            import tempfile
            from unittest.mock import Mock
            with tempfile.TemporaryDirectory() as directory:
                path=Path(directory)/'stderr.log';path.write_text('pending buffered native end')
                process=Mock(pid=10)
                def normal_exit(timeout):
                    path.write_text('ROCKET_WORLD_QA_END {"reason":"route_observed"}')
                    return 0
                process.wait.side_effect=normal_exit
                with patch('test_boss_network_windows.window_for_pid', return_value=None):
                    self.assertTrue(closed_process_completion(process,path,lambda text: expected_shutdown(text,'jrb-water')))
                with patch('test_boss_network_windows.window_for_pid', return_value=1):
                    process.wait.reset_mock()
                    self.assertFalse(closed_process_completion(process,path,lambda text: True))
                    process.wait.assert_not_called()
                with patch('test_boss_network_windows.window_for_pid', return_value=None):
                    process.wait.side_effect=None;process.wait.return_value=1
                    self.assertFalse(closed_process_completion(process,path,lambda text: True))
                    process.wait.return_value=0;path.write_text('no completed run')
                    self.assertFalse(closed_process_completion(process,path,lambda text: expected_shutdown(text,'jrb-water')))

        def test_progression_only_targets_the_owned_wing_driver(self):
            value=args('wing');value.cap_progression=True
            _, driver_env, _=launch_plan('host',value,Path('host'),{})
            _, observer_env, _=launch_plan('client',value,Path('client'),{})
            self.assertEqual(driver_env['SM64_ROCKET_QA_CAP_PROGRESS'],'wing')
            self.assertNotIn('SM64_ROCKET_QA_CAP_PROGRESS',observer_env)

        def test_help_is_launch_free(self):
            with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as stopped_help:
                main(['--help'])
            self.assertEqual(stopped_help.exception.code, 0)

    with patch('subprocess.Popen', side_effect=AssertionError('Headless self-tests must not launch a process')):
        result = unittest.TextTestRunner(verbosity=1).run(unittest.defaultTestLoader.loadTestsFromTestCase(RunnerTests))
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
