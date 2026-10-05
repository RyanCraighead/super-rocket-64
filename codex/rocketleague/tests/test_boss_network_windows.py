"""Opt-in, muted local peers. Only the owned driver gets virtual SDL input."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from boss_network_acceptance import analyze

ROOT = Path(__file__).resolve().parents[3]


def digest(path, algorithm='sha256'):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, algorithm).hexdigest()


def window_for_pid(pid):
    user = ctypes.windll.user32
    user.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
    found = []
    callback = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    @callback
    def visit(window, _):
        owner = ctypes.c_ulong()
        user.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == pid and user.IsWindowVisible(ctypes.c_void_p(window)):
            found.append(window)
            return False
        return True

    user.EnumWindows(visit, None)
    return found[0] if found else None


def foreground_pid():
    user = ctypes.windll.user32
    user.GetForegroundWindow.restype = ctypes.c_void_p
    user.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
    owner = ctypes.c_ulong()
    user.GetWindowThreadProcessId(user.GetForegroundWindow(), ctypes.byref(owner))
    return owner.value


def foreground_diagnostic(driver):
    """Record window identity, without capturing another application's content."""
    user = ctypes.windll.user32
    user.GetForegroundWindow.restype = ctypes.c_void_p
    user.GetClassNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int]
    window = user.GetForegroundWindow()
    name = ctypes.create_unicode_buffer(256)
    user.GetClassNameW(window, name, len(name))
    return dict(foreground_pid=foreground_pid(), foreground_class=name.value,
                driver_pid=driver.pid, driver_window_present=bool(window_for_pid(driver.pid)),
                driver_exit_before_stop=driver.poll())


def closed_process_completion(process, log_path, completed):
    """Let a destroyed-window process flush its log before judging shutdown."""
    if window_for_pid(process.pid):
        return False
    try:
        if process.wait(timeout=2) != 0:
            return False
        return bool(completed(log_path.read_text(errors='replace')))
    except (subprocess.TimeoutExpired, OSError):
        return False


def focus(pid):
    user, kernel = ctypes.windll.user32, ctypes.windll.kernel32
    user.GetForegroundWindow.restype = ctypes.c_void_p
    user.SetForegroundWindow.argtypes = [ctypes.c_void_p]
    user.ShowWindow.argtypes = [ctypes.c_void_p, ctypes.c_int]
    user.BringWindowToTop.argtypes = [ctypes.c_void_p]
    window = window_for_pid(pid)
    if not window:
        return False
    owner = ctypes.c_ulong()
    foreground = user.GetWindowThreadProcessId(user.GetForegroundWindow(), ctypes.byref(owner))
    own = kernel.GetCurrentThreadId()
    joined = foreground != own and user.AttachThreadInput(own, foreground, True)
    try:
        user.ShowWindow(window, 9)
        user.BringWindowToTop(window)
        return bool(user.SetForegroundWindow(window))
    finally:
        if joined:
            user.AttachThreadInput(own, foreground, False)


def rows(path):
    return [json.loads(line.removeprefix('BOSS_NET_NATIVE '))
            for line in path.read_text(errors='replace').splitlines(keepends=True)
            if line.endswith('\n') and line.startswith('BOSS_NET_NATIVE ')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'rom', 'octane', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--driver', choices=('host', 'client'), required=True)
    parser.add_argument('--scenario', choices=('bob', 'bob-recovery', 'bowser'), required=True)
    parser.add_argument('--mario-driver', action='store_true')
    parser.add_argument('--observer-car', action='store_true')
    parser.add_argument('--observer-outside', action='store_true')
    parser.add_argument('--reconnect-observer', action='store_true')
    parser.add_argument('--orbit-delay-ms', type=int, default=900)
    parser.add_argument('--port', type=int, default=27916)
    parser.add_argument('--boost-mode', choices=('finite', 'infinite'), default='finite')
    parser.add_argument('--allow-focused-test', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt' or not args.allow_focused_test:
        parser.error('An explicitly coordinated Windows foreground test is required')
    if args.reconnect_observer and args.driver != 'host':
        parser.error('The observer must be the client for a reconnect test')
    if not 0 <= args.orbit_delay_ms <= 2500:
        parser.error('Orbit delay must be 0..2500 ms')
    if args.mario_driver and (args.scenario == 'bob-recovery' or not args.observer_car):
        parser.error('Native Mario checks use bob/bowser with an Octane observer')
    if not 1024 <= args.port <= 65535:
        parser.error('Invalid local test port')
    for name in ('binary', 'rom', 'octane', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    if digest(args.binary) != args.sha256:
        parser.error('Binary hash changed')
    if digest(args.rom, 'sha1') != '9bef1128717f958171a4afac3ed78ee2bb4e86ce':
        parser.error('Supported privately supplied SM64 ROM required')
    if not args.output.is_relative_to((ROOT / 'codex/.runtime').resolve()):
        parser.error('Use a fresh private codex/.runtime directory')
    existing = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
        'Get-Process sm64coopdx* -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id'],
        capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    if existing.stdout.strip():
        parser.error('An SM64 session is running; preserve it')
    args.output.mkdir(parents=True, exist_ok=False)
    processes, logs = [], []
    focused = False

    def launch(role, suffix=''):
        directory = args.output / (role + suffix)
        save = directory / 'save'
        save.mkdir(parents=True, exist_ok=False)
        shutil.copyfile(args.rom, save / 'baserom.us.z64')
        driving = role == args.driver
        (save / 'sm64config.txt').write_text(
            'window_w 1280\nwindow_h 720\nfullscreen false\nvsync false\nframe_limit 30\n'
            'framerate_mode 1\ninterpolation_mode 1\nmaster_volume 0\nmusic_volume 0\n'
            'sfx_volume 0\nenv_volume 0\nbackground_gamepad 0\nbettercam_enable true\n'
            f'bettercam_analog true\ndisable_gamepads {"false" if driving else "true"}\n'
            f'rocket_boost_mode {int(role == "host" and args.boost_mode == "infinite")}\n')
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(('SM64_ROCKET_QA_', 'SM64_CHARACTER_NET_', 'SM64_BOSS_NET_'))
               and key not in ('THPS_SCORE_SCENARIO', 'SM64_QA_OUTPUT_DIR')}
        env.update(SDL_AUDIODRIVER='dummy', SM64_CHARACTER_NET_MOTION='1',
                   SM64_CHARACTER_NET_OBSERVE=str(directory), SM64_CHARACTER_NET_SECONDS='230',
                   SM64_BOSS_NET_OBSERVE='outside' if args.observer_outside else args.scenario)
        command = [str(args.binary), '--console', '--disable-mods', '--hide-loading-screen',
            '--skip-intro', '--skip-update-check', '--no-discord', '--windowed',
            '--savepath', str(save), '--character-net', str(args.octane), '--loopback-only',
            '--playername', 'Boss-' + role, '--backend', 'opengl']
        command += ['--server', str(args.port)] if role == 'host' else ['--client', '::1', str(args.port)]
        if (driving and not args.mario_driver) or (not driving and args.observer_car):
            command += ['--rocket-car', str(args.octane)]
        if driving:
            env.update(SM64_ROCKET_QA_OUTPUT=str(directory), SM64_ROCKET_QA_GAMEPAD='1')
            env['SM64_ROCKET_QA_BOSS_ORBIT_DELAY'] = str(args.orbit_delay_ms)
            if args.reconnect_observer:
                env['SM64_ROCKET_QA_BOSS_RECONNECT'] = '1'
            env['SM64_ROCKET_QA_MARIO_BOSS' if args.mario_driver else 'SM64_ROCKET_QA_BOSS'] = args.scenario
            env['SM64_BOSS_NET_OBSERVE'] = 'outside'  # driver stages itself once
        out, err = (directory / 'stdout.log').open('w'), (directory / 'stderr.log').open('w')
        logs.extend((out, err))
        process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=out, stderr=err,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        entry = dict(process=process, role=role, directory=directory, driving=driving)
        processes.append(entry)
        print(f'Native boss {role}{suffix}: PID {process.pid}, driver={driving}', flush=True)
        return entry

    try:
        host = launch('host')
        time.sleep(3)
        client = launch('client')
        driver = host if args.driver == 'host' else client
        observer = client if args.driver == 'host' else host
        # Wait for the owned window, then make a single initial focus request.
        # Never repeatedly reclaim focus from the person using the desktop.
        for _ in range(100):
            if driver['process'].poll() is not None or window_for_pid(driver['process'].pid):
                break
            time.sleep(.1)
        focused = (foreground_pid() == driver['process'].pid or focus(driver['process'].pid))
        focus_lost = not focused
        focus_diagnostic = None
        stop_requested = False
        reconnected = False
        start = time.monotonic()
        while not focus_lost and driver['process'].poll() is None and time.monotonic() - start < 225:
            time.sleep(.2)
            if driver['process'].poll() is not None:
                break
            stop_requested = (args.output / 'stop-all').exists() or (ROOT.parent / 'STOP-NATIVE-QA').exists()
            if stop_requested or foreground_pid() != driver['process'].pid:
                # SDL destroys its window before the process handle signals.
                # A recorded completion plus a destroyed window and clean exit
                # is expected shutdown, not loss of focus during gameplay.
                if not stop_requested and closed_process_completion(driver['process'],
                        driver['directory'] / 'stderr.log', lambda log: any(
                            line.startswith(('MARIO_BOSS_QA_END ', 'ROCKET_BOSS_QA_END '))
                            for line in log.splitlines())):
                    break
                focus_diagnostic = foreground_diagnostic(driver['process'])
                focus_diagnostic['elapsed_seconds'] = round(time.monotonic() - start, 3)
                # A crash can remove the window just before the process signals.
                # Preserve its exit code instead of replacing it with terminate().
                if not stop_requested and not focus_diagnostic['driver_window_present']:
                    try:
                        focus_diagnostic['driver_exit_before_stop'] = driver['process'].wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        pass
                focus_lost = not stop_requested
                print('Stopping owned peers: ' + ('stop request' if stop_requested else 'unexpected focus loss'), flush=True)
                print('FOCUS_DIAGNOSTIC ' + json.dumps(focus_diagnostic), flush=True)
                break
            if args.reconnect_observer and not reconnected:
                samples = rows(driver['directory'] / 'stderr.log')
                damaged = any(row['health'] == (0 if args.scenario == 'bowser' else 2) for row in samples)
                if damaged:
                    (observer['directory'] / 'stop').touch()
                    observer['process'].wait(timeout=10)
                    observer = launch('client', '-reconnected')
                    reconnected = True
                    # The new owned observer may take focus during its creation.
                    # Restore only from that known window, never from another app.
                    for _ in range(100):
                        if observer['process'].poll() is not None or window_for_pid(observer['process'].pid):
                            break
                        time.sleep(.1)
                    owner = foreground_pid()
                    if owner == observer['process'].pid:
                        focus_lost = not focus(driver['process'].pid)
                    elif owner != driver['process'].pid:
                        focus_lost = True
        timed_out = driver['process'].poll() is None and not focus_lost and not stop_requested
        if (focus_lost or stop_requested) and driver['process'].poll() is None:
            driver['process'].terminate()
        (observer['directory'] / 'stop').touch()
        try:
            observer['process'].wait(timeout=10)
        except subprocess.TimeoutExpired:
            observer['process'].terminate()
        if driver['process'].poll() is None:
            driver['process'].terminate()
        for entry in processes:
            entry['process'].wait(timeout=10)
        source_log = (driver['directory'] / 'stderr.log').read_text(errors='replace')
        peers = [entry for entry in processes if entry is not driver]
        peer_logs = [(entry['directory'] / 'stderr.log').read_text(errors='replace') for entry in peers]
        all_remote = [row for entry in peers for row in rows(entry['directory'] / 'stderr.log')]
        source = rows(driver['directory'] / 'stderr.log')
        if args.mario_driver:
            from mario_boss_acceptance import analyze as analyze_mario
            gameplay = analyze_mario(source_log, args.scenario)
        else:
            gameplay = analyze(source_log, peer_logs, args.scenario, source[0]['global'] if source else -1)
        remote = rows(observer['directory'] / 'stderr.log')
        peer_log = (observer['directory'] / 'stderr.log').read_text(errors='replace')
        checks = dict(driver_complete=gameplay['passed'],
            native_processes_ok=all(entry['process'].returncode == 0 for entry in processes),
            driver_has_authority=any(row['global'] == row['owner'] for row in source),
            no_duplicate_rewards=all(row['rewards'] <= 1 for row in source + all_remote),
            focused=focused, uninterrupted_focus=not focus_lost,
            no_stop_request=not stop_requested, no_timeout=not timed_out,
            reconnect_exercised=not args.reconnect_observer or reconnected)
        if args.mario_driver:
            checks['no_car_attacks'] = 'ROCKET_BOSS_IMPACT' not in source_log + ''.join(peer_logs)
        if args.scenario == 'bowser':
            checks['camera_telemetry_present'] = bool(source) and all(
                all(type(row.get(key)) is int for key in ('camera_cutscene', 'dialog', 'player_action'))
                for row in source + all_remote)
            checks['intro_camera_owned'] = all(row.get('camera_cutscene') != 144 or
                row['global'] == row['owner'] for row in source + all_remote)
            checks['follower_intro_finished'] = all(row.get('camera_cutscene') != 144 and
                row.get('dialog') not in (67, 92, 93) for row in all_remote if row['health'] == 0)
        if not args.observer_outside:
            escape = args.mario_driver and args.scenario == 'bob'
            checks.update(remote_state_received=len(remote) > 30,
                remote_damage=any(row['health'] == (3 if args.scenario == 'bob-recovery' or escape else 0)
                                  and row['revision'] > 0 for row in remote),
                remote_reward=args.mario_driver or args.scenario == 'bob-recovery' or any(row['rewards'] == 1 for row in remote))
            if args.mario_driver:
                native_phase = 3 if escape else 1
                first = next((i for i, row in enumerate(remote) if row['action'] == native_phase), None)
                checks['remote_native_transition'] = first is not None and any(
                    row['action'] == 2 if escape else row['health'] == 0 for row in remote[first + 1:])
                if escape:
                    checks['escape_preserves_health'] = all(row['health'] == 3 and row['rewards'] == 0 for row in source + remote)
        result = dict(schema='octane-native-boss-network-v1', binary_sha256=args.sha256,
            driver=args.driver, scenario=args.scenario, mario_driver=args.mario_driver, observer_car=args.observer_car,
            observer_outside=args.observer_outside, reconnect_observer=args.reconnect_observer,
            orbit_delay_ms=args.orbit_delay_ms,
            host_boost_mode=args.boost_mode,
            focus_diagnostic=focus_diagnostic,
            checks=checks, passed=all(checks.values()), driver_gameplay=gameplay,
            processes=[dict(role=e['role'], directory=e['directory'].name,
                       pid=e['process'].pid, exit_code=e['process'].returncode) for e in processes],
            muted=True, loopback_only=True, internet_tested=False,
            direct_actor_state_writes=False, input='owned driver virtual SDL controller only')
        (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return 0 if result['passed'] else 1
    finally:
        for entry in processes:
            if entry['process'].poll() is None:
                entry['process'].terminate()
                entry['process'].wait(timeout=10)
        for stream in logs:
            stream.close()


if __name__ == '__main__':
    raise SystemExit(main())
