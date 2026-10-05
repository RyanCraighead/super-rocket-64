"""Read native Wing progression evidence; never drives or changes the game.

Exit 0 requires the complete narrow switch/box/pickup/expiry route. Synthetic
fixtures are analyzer tests, not native evidence. Visual/remote acceptance is
always separate, even when every route check passes.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path


def _object(pairs):
    out = {}
    for key, value in pairs:
        if key in out:
            raise ValueError('Duplicate telemetry key: ' + key)
        out[key] = value
    return out


def _records(text, name):
    prefix = 'ROCKET_CAP_PROGRESS_' + name + ' '
    return [json.loads(line[len(prefix):], object_pairs_hook=_object,
                       parse_constant=lambda value: (_ for _ in ()).throw(ValueError('Nonfinite JSON')))
            for line in text.splitlines() if line.startswith(prefix)]


def analyze(text, directory=None):
    try:
        return _analyze(text, directory)
    except (KeyError, TypeError, ValueError, IndexError, OSError, OverflowError) as error:
        return dict(schema='rocket-cap-progress-v1', passed=False,
                    checks={'well_formed_evidence': False}, error=str(error),
                    full_feature_acceptance=False)


def _analyze(text, directory):
    starts, stages, ends = (_records(text, k) for k in ('BEGIN', 'STAGE', 'END'))
    states, captures = _records(text, 'STATE'), _records(text, 'CAPTURE')
    if len(starts) != 1 or len(ends) != 1:
        raise ValueError('Require one start and one explicit end')
    meta, end = starts[0], ends[0]
    if meta['scenario'] != 'wing' or meta['actor_writes'] != 0 or meta['muted'] != 1:
        raise ValueError('Incompatible driver admission')
    if type(end['ms']) is not int or not 0 <= end['ms'] <= 130000:
        raise ValueError('Run exceeds the bounded observation window')
    vector_sizes = dict(pos=3, vel=3, cap_pos=3, sent=6, switch=4, box=4, cap_id=2)
    integer_fields = ('ms', 'timer', 'level', 'phase', 'active', 'ground', 'wheels', 'platform',
                      'action', 'health', 'dialog', 'save', 'caps', 'visual_caps', 'cap_timer',
                      'managed', 'online', 'epoch', 'ticks', 'topper', 'cap_valid', 'cap_active',
                      'cap_age', 'cap_intangible', 'cap_status', 'cap_parent', 'interact_cap')
    for row in states:
        if any(type(row[key]) is not int for key in integer_fields):
            raise ValueError('Malformed integer telemetry')
        for key, length in vector_sizes.items():
            values = row[key]
            if not isinstance(values, list) or len(values) != length or any(
                    type(v) not in (int, float) or not math.isfinite(v) for v in values):
                raise ValueError('Malformed finite vector: ' + key)
        if not math.isfinite(row['fuel']) or row['level'] != meta['target_level']:
            raise ValueError('Nonfinite fuel or wrong native level')
        if row['online'] not in (0, 1) or row['managed'] not in (0, 1) or row['role'] not in ('none', 'server', 'client'):
            raise ValueError('Invalid lease/role telemetry')
        if row['online'] and row['role'] == 'none':
            raise ValueError('Online sample has no network role')
    if any(b['timer'] <= a['timer'] or b['ms'] < a['ms'] for a, b in zip(states, states[1:])):
        raise ValueError('Native frame sequence is not increasing')
    if states and (states[-1]['ms'] > end['ms'] or states[0]['ms'] < 0):
        raise ValueError('State timestamp lies outside run')
    if len(captures) > 3 or any(c['ms'] > end['ms'] for c in captures):
        raise ValueError('Capture budget or timestamp exceeded')
    frames = []
    if directory is not None:
        for row in captures:
            path = Path(directory) / f"frame-{row['ms']:05d}.ppm"
            data = path.read_bytes()
            if not data.startswith(b'P6\n'):
                raise ValueError('Missing P6 native capture')
            frames.append(dict(file=path.name, sha256=hashlib.sha256(data).hexdigest()))
    for key in ('switch', 'box', 'cap_id'):
        identities = {tuple(s[key][:2]) for s in states if s[key][0] >= 0}
        if len(identities) > 1:
            raise ValueError('Native actor identity changed: ' + key)

    save_flag, wing = meta['save_flag'], meta['wing_flag']
    fresh = not (meta['initial_save'] & save_flag)
    unlock = None
    for i, row in enumerate(states[1:], 1):
        before = states[i-1]
        if (not before['save'] & save_flag and row['save'] & save_flag and
                before['switch'][0] >= 0 and before['switch'][:2] == row['switch'][:2] and
                before['switch'][2] == 1 and row['switch'][2] == 2):
            supported = any(s['active'] and s['ground'] and s['wheels'] > 0 and
                            s['platform'] == row['switch'][0] and row['timer'] - s['timer'] <= 3
                            for s in states[max(0, i-3):i+1])
            if supported:
                unlock = i
                break
    dialog_done = next((i for i, s in enumerate(states) if unlock is not None and i > unlock and
                        s['switch'][2] == 3 and s['dialog'] == -1 and s['active']), None)
    box_break = None
    if dialog_done is not None:
        for i in range(dialog_done+1, len(states)):
            a, b = states[i-1:i+1]
            if a['box'][0] >= 0 and a['box'][:2] == b['box'][:2] and a['box'][2] == 2 and b['box'][2] == 3:
                box_break = i
                break
    spawn = next((i for i, s in enumerate(states) if box_break is not None and i >= box_break and
                  s['cap_valid'] and s['cap_active'] and s['cap_id'][0] >= 0 and
                  s['cap_parent'] == s['box'][0] and s['box'][2] in (4, 5)), None)

    pickup = None
    # Host collection and lease packets can arrive in either order. Require
    # the same observed child to disappear near the active car and a local
    # timer renewal beyond the starter's maximum, within one second.
    if spawn is not None:
        for i in range(spawn+1, len(states)):
            a, b = states[i-1:i+1]
            if not (a['cap_active'] and not b['cap_active'] and a['cap_id'] == b['cap_id']):
                continue
            near = any(s['active'] and s['cap_active'] and s['cap_intangible'] == 0 and
                       b['ms'] - s['ms'] <= 300 and math.dist(s['pos'], s['cap_pos']) < 300
                       for s in states[max(spawn, i-9):i])
            if not near:
                continue
            # The native child can spawn and renew the recipient's lease in
            # the first observed frame, before unlinking on the next frame.
            # Include that frame's real before/after timer transition.
            for j in range(max(1, spawn, i-30), min(len(states), i+31)):
                previous, now = states[j-1:j+1]
                if (abs(now['ms']-b['ms']) <= 1000 and now['cap_timer'] > meta['entry_duration'] and
                        now['cap_timer'] > previous['cap_timer'] + 100 and now['caps'] & wing and
                        ((now['online'] and now['managed']) or
                         (not now['online'] and any(s['interact_cap'] for s in states[max(i, j):min(len(states), max(i, j)+4)])))):
                    pickup = max(i, j)
                    break
            if pickup is not None:
                break
    expired = next((i for i, s in enumerate(states) if pickup is not None and i > pickup and
                    not s['caps'] & wing and s['cap_timer'] == 0), None)
    cap_rows = states[pickup:expired] if pickup is not None and expired is not None else []
    after = states[expired:] if expired is not None else []
    countdown = len(cap_rows) > 1 and all(0 < s['cap_timer'] <= meta['pickup_duration'] and s['caps'] & wing for s in cap_rows)
    countdown = countdown and all(b['cap_timer'] <= a['cap_timer'] for a, b in zip(cap_rows, cap_rows[1:]))
    # Deductions follow native frame time, not a driver timeout or actor delete.
    expiry_duration = bool(cap_rows) and after[0]['timer'] - cap_rows[0]['timer'] >= cap_rows[0]['cap_timer'] - 2
    expiry_duration = expiry_duration and all(0 <= a['cap_timer'] - b['cap_timer'] <= b['timer'] - a['timer'] + 1
        for a, b in zip(cap_rows, cap_rows[1:] + after[:1]))
    online_cap = [s for s in cap_rows if s['online']]
    capture_events = dict(switch_unlocked=unlock, native_cap_collected=pickup, post_pickup_expired=expired)
    capture_names = [c['reason'] for c in captures]
    capture_timing = sorted(capture_names) == sorted(capture_events) and all(
        capture_events[c['reason']] is not None and
        0 <= c['ms'] - states[capture_events[c['reason']]]['ms'] <= 2500 for c in captures)
    checks = dict(
        well_formed_evidence=True,
        admitted_fresh_save=fresh,
        one_normal_entry=len(stages) == 1 and stages[0]['normal_entry'] == 1 and stages[0]['level'] == meta['target_level'],
        finished=end.get('complete') == 1 and end.get('reason') == 'bounded_observation_complete',
        switch_pressed_by_car=unlock is not None,
        unlock_persisted=unlock is not None and all(s['save'] & save_flag for s in states[unlock:]),
        native_dialog_finished=dialog_done is not None,
        unlocked_box_broken=box_break is not None,
        native_child_spawned=spawn is not None,
        native_child_collected=pickup is not None,
        managed_online_pickup=(not any(s['online'] for s in states)) or
                              (bool(online_cap) and all(s['managed'] for s in online_cap)),
        natural_post_pickup_expiry=bool(countdown and expiry_duration and after),
        post_expiry_visual_clear=bool(after) and all(not s['visual_caps'] & wing and not s['topper'] for s in after),
        control_resumed=bool(after) and any(s['active'] and s['ground'] and s['sent'][0] > 0 and
                            s['ticks'] > after[0]['ticks'] and s['ms'] >= after[0]['ms'] + 500 and
                            math.hypot(s['pos'][0]-after[0]['pos'][0], s['pos'][2]-after[0]['pos'][2]) > 10 for s in after),
        meaningful_capture_records=capture_timing,
    )
    pending = ['Native route results need actual execution; synthetic fixtures are not gameplay evidence',
               'Human review of switch, cap/topper and HUD screenshots',
               'Paired host/client convergence, reconnect and duplicate-grant acceptance',
               'Physical controllers and remapped bindings',
               'Metal/Vanish progression and real barrier recovery']
    return dict(schema='rocket-cap-progress-v1', passed=all(checks.values()), checks=checks,
                full_feature_acceptance=False, stop_reason=end['reason'], pending=pending,
                frames=len(states), screenshots=frames, screenshot_files_verified=directory is not None,
                log_sha256=hashlib.sha256(text.encode()).hexdigest(),
                event_frames=dict(unlock=states[unlock]['timer'] if unlock is not None else None,
                                  pickup=states[pickup]['timer'] if pickup is not None else None,
                                  expiry=states[expired]['timer'] if expired is not None else None))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--directory', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    result = analyze(args.log.read_text(errors='replace'), args.directory)
    encoded = json.dumps(result, indent=2) + '\n'
    if args.out:
        args.out.write_text(encoded)
    print(encoded)
    raise SystemExit(0 if result['passed'] else 1)
