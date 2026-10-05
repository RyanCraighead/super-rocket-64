"""Read-only native Mario QA evidence checks, not a gameplay simulation.

The input is one driver's stderr text. The runner must separately check the
passive peer's canonical state, process exit status, and real captured frames.
Action values below are the native constants in include/sm64.h.
"""
import json
import math
import re

ACT_GRABBED = 0x00020370
ACT_HOLDING_BOWSER = 0x00000391
ACT_RELEASING_BOWSER = 0x00000392


def _object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON key')
        result[key] = value
    return result


def _json(text):
    def nonfinite(_):
        raise ValueError('Nonfinite JSON value')
    value = json.loads(text, object_pairs_hook=_object, parse_constant=nonfinite)
    if not isinstance(value, dict):
        raise ValueError('Expected JSON object')
    def finite_tree(item):
        if isinstance(item, dict):
            for child in item.values():
                finite_tree(child)
        elif isinstance(item, list):
            for child in item:
                finite_tree(child)
        elif isinstance(item, float) and not math.isfinite(item):
            raise ValueError('Nonfinite JSON value')
    finite_tree(value)
    return value


def _integer(record, key, low=0, high=0xFFFFFFFF):
    value = record[key]
    if type(value) is not int or not low <= value <= high:
        raise ValueError('Invalid integer: ' + key)
    return value


def _number(value):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError('Expected finite number')
    return value


def _vector(record, key, length=3):
    value = record[key]
    if not isinstance(value, list) or len(value) != length:
        raise ValueError('Invalid vector: ' + key)
    for item in value:
        _number(item)
    return value


def _state(value, scenario):
    if value['scenario'] != scenario:
        raise ValueError('Mixed driver scenarios')
    for key in ('ms', 'frame', 'action', 'action_arg', 'epoch', 'waypoint'):
        _integer(value, key)
    for key in ('health', 'boss_health', 'boss_action', 'boss_sub', 'escape'):
        _integer(value, key, 0, 32767)
    for key in ('held', 'grabbed', 'owner', 'blocked'):
        _integer(value, key, 0, 1)
    _integer(value, 'yaw', -32768, 32767)
    _integer(value, 'spin', -32768, 32767)
    _vector(value, 'pos')
    _vector(value, 'boss_pos')
    pad = _vector(value, 'pad', 4)
    if any(abs(axis) > 1.0001 for axis in pad[:2]) or any(
            type(button) is not int or button not in (0, 1) for button in pad[2:]):
        raise ValueError('Invalid virtual input')
    if value['blocked'] and any(pad):
        raise ValueError('Virtual input during blocked focus/UI')
    if value['owner'] and not value['epoch']:
        raise ValueError('Owner without an epoch')


def analyze(log, scenario):
    """Return a JSON-serializable verdict; malformed/incomplete logs fail closed."""
    try:
        if not isinstance(log, str) or scenario not in ('bob', 'bowser'):
            raise ValueError('Unknown scenario or invalid log')
        return _analyze(log, scenario)
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        return {'passed': False, 'checks': {'well_formed_telemetry': False},
                'acceptance': {}, 'state_count': 0, 'error': str(error)}


def _analyze(log, scenario):
    states, natives, mines, requests, ends = [], [], [], [], []
    begins = stages = 0
    last_ms = 0
    impact = reward_marker = False
    for line_number, line in enumerate(log.splitlines()):
        if line.startswith('ROCKET_BOSS_IMPACT'):
            impact = True
        if line.startswith('ROCKET_BOSS_QA_REWARD'):
            reward_marker = True
        if line.startswith('BOSS_NET_NATIVE '):
            value = _json(line.split(' ', 1)[1])
            for key in ('frame', 'epoch', 'revision', 'level', 'kind', 'health',
                        'action', 'sub', 'held', 'rewards'):
                _integer(value, key)
            _integer(value, 'global', 0, 255)
            _integer(value, 'owner', 0, 255)
            _integer(value, 'frozen', 0, 1)
            _vector(value, 'pos')
            value['_line'] = line_number
            natives.append(value)
        if not line.startswith('MARIO_BOSS_QA_'):
            continue
        if ends:
            raise ValueError('Driver evidence after completion')
        marker, separator, payload = line.partition(' ')
        if not separator:
            raise ValueError('Truncated driver marker')
        if marker == 'MARIO_BOSS_QA_BEGIN':
            if payload != f'scenario={scenario} input=virtual_sdl_only' or begins or stages:
                raise ValueError('Invalid or repeated begin')
            begins += 1
            continue
        if not begins:
            raise ValueError('Driver evidence before begin')
        if marker == 'MARIO_BOSS_QA_STAGE':
            expected = 'level=30 node=-1' if scenario == 'bowser' else 'level=9 node=11'
            if stages or payload != expected:
                raise ValueError('Invalid or repeated staging')
            stages += 1
            continue
        if not stages:
            raise ValueError('Driver evidence before staging')
        if marker == 'MARIO_BOSS_QA_RELEASE_REQUEST':
            match = re.fullmatch(r'ms=(\d+) spin=(-?\d+) yaw=(-?\d+) error=([^ ]+) mine=\[([^,]+),([^,]+),([^\]]+)\]', payload)
            if not match:
                raise ValueError('Malformed release request')
            ms, spin, yaw, error, mx, my, mz = match.groups()
            value = dict(ms=int(ms), spin=int(spin), yaw=int(yaw), error=float(error),
                         target=[float(mx), float(my), float(mz)])
            _integer(value, 'ms')
            _integer(value, 'spin', -32768, 32767)
            _integer(value, 'yaw', -32768, 32767)
            _number(value['error'])
            _vector(value, 'target')
            if value['ms'] < last_ms:
                raise ValueError('Driver time moved backward')
            last_ms = value['ms']
            value['_line'] = line_number
            requests.append(value)
            continue
        value = _json(payload)
        _integer(value, 'ms')
        if value['ms'] < last_ms:
            raise ValueError('Driver time moved backward')
        last_ms = value['ms']
        value['_line'] = line_number
        if marker == 'MARIO_BOSS_QA_STATE':
            _state(value, scenario)
            if states and (value['ms'] < states[-1]['ms'] or value['frame'] < states[-1]['frame']):
                raise ValueError('State time moved backward')
            states.append(value)
        elif marker == 'MARIO_BOSS_QA_MINE':
            _integer(value, 'ms')
            _vector(value, 'pos')
            _vector(value, 'target')
            mines.append(value)
        elif marker == 'MARIO_BOSS_QA_END':
            if value['scenario'] != scenario or not isinstance(value['reason'], str):
                raise ValueError('Malformed completion')
            for key in ('passed', 'authority', 'grab', 'release', 'native_throw', 'mine', 'damage', 'escape'):
                _integer(value, key, 0, 1)
            for key in ('max_spin', 'max_escape', 'ms'):
                _integer(value, key)
            ends.append(value)
        else:
            raise ValueError('Unexpected or rejected QA marker: ' + marker)

    end = ends[0] if len(ends) == 1 else {}
    expected_kind = 2 if scenario == 'bowser' else 1
    relevant = [n for n in natives if n['kind'] == expected_kind]
    checks = dict(well_formed_telemetry=True,
                  finished=len(ends) == 1 and end.get('passed') == 1 and begins == stages == 1,
                  native_states=bool(states) and bool(relevant), no_car_impacts=not impact,
                  authoritative_grab=end.get('authority') == end.get('grab') == 1,
                  mario_survived=bool(states) and all(s['health'] >= 0x100 for s in states),
                  completion_after_evidence=bool(states) and end.get('ms', -1) >= states[-1]['ms'],
                  one_native_player=bool(relevant) and len({n['global'] for n in relevant}) == 1)

    def owns(state):
        return state['owner'] == 1 and state['epoch'] > 0 and not state['blocked']

    def find(predicate, start=0):
        return next((i for i in range(start, len(states)) if predicate(states[i])), None)

    if scenario == 'bob':
        grab = find(lambda s: owns(s) and s['grabbed'] and s['action'] == ACT_GRABBED and s['boss_action'] == 3)
        escape = find(lambda s: owns(s) and s['escape'] > 10 and s['boss_action'] == 2,
                      grab + 1 if grab is not None else len(states))
        resume = find(lambda s: owns(s) and not s['grabbed'] and not s['held'] and s['action'] != ACT_GRABBED
                      and s['boss_action'] == 2 and s['ms'] >= states[escape]['ms'] + 700,
                      escape + 1 if escape is not None else len(states))
        checks.update(
            native_grab_escape_resume=grab is not None and escape is not None and resume is not None,
            grab_authority_continuous=grab is not None and escape is not None and all(
                owns(s) and s['epoch'] == states[grab]['epoch'] for s in states[grab:escape + 1]),
            escape_input=grab is not None and escape is not None and any(
                s['grabbed'] and (s['pad'][2] or abs(s['pad'][0]) > .4) for s in states[grab:escape]),
            native_owner_grab=grab is not None and any(n['owner'] == n['global'] and not n['frozen']
                and n['epoch'] == states[grab]['epoch'] and n['action'] == 3 for n in relevant),
            health_preserved=bool(states) and all(s['boss_health'] == 3 for s in states)
                and bool(relevant) and all(n['health'] == 3 for n in relevant),
            no_rewards=not reward_marker and all(n['rewards'] == 0 for n in relevant),
            no_throw_or_mine=not mines and not requests and all(s['boss_action'] != 4 for s in states)
                and all(n['action'] != 4 for n in relevant),
            completion_agrees=end.get('reason') == 'native_grab_button_escape' and end.get('escape') == 1
                and end.get('max_escape', 0) > 10 and all(end.get(k) == 0 for k in
                    ('release', 'native_throw', 'mine', 'damage')),
        )
    else:
        held = find(lambda s: owns(s) and s['held'] and s['action'] == ACT_HOLDING_BOWSER and s['boss_health'] == 1)
        release = find(lambda s: owns(s) and s['action'] == ACT_RELEASING_BOWSER and s['action_arg'] == 0,
                       held + 1 if held is not None else len(states))
        thrown = find(lambda s: owns(s) and s['boss_action'] == 1 and s['boss_health'] == 1,
                      release if release is not None else len(states))
        damage = find(lambda s: owns(s) and s['boss_health'] == 0 and s['boss_action'] == 4,
                      thrown + 1 if thrown is not None else len(states))
        aimed = [r for r in requests if held is not None and release is not None
                 and states[held]['_line'] < r['_line'] < states[release]['_line']
                 and states[held]['ms'] <= r['ms'] <= states[release]['ms']
                 and 0xE00 <= abs(r['spin']) <= 0x1000 and abs(r['error']) < .22]
        def native_mine_interval(mine):
            if thrown is None or damage is None:
                return False
            first, hit = states[thrown], states[damage]
            if not (first['_line'] < mine['_line'] and first['ms'] <= mine['ms']):
                return False
            if mine['_line'] < hit['_line'] and mine['ms'] <= hit['ms']:
                return True
            # Bowser decrements health when he detects the mine. The mine's
            # separate native behavior may spawn its explosion next frame.
            # Require a subsequent observed state in that exact same/next frame,
            # with continuous authority and terminal damage; late effects fail.
            observed = next((s for s in states if s['_line'] > mine['_line']), None)
            return bool(observed and 0 <= observed['frame'] - hit['frame'] <= 1
                        and hit['ms'] <= mine['ms'] <= observed['ms'] <= hit['ms'] + 100
                        and owns(observed) and observed['epoch'] == hit['epoch']
                        and observed['boss_health'] == 0 and observed['boss_action'] == 4)

        matched_mines = [m for m in mines if native_mine_interval(m)
                         and math.hypot(m['pos'][0] - m['target'][0], m['pos'][2] - m['target'][2]) < 200
                         and any(all(abs(m['target'][j] - r['target'][j]) <= .11 for j in range(3)) for r in aimed)]
        checks.update(
            native_hold_release_throw_damage=all(index is not None for index in (held, release, thrown, damage)),
            throw_authority_continuous=held is not None and damage is not None and all(
                owns(s) and s['epoch'] == states[held]['epoch'] for s in states[held:damage + 1]),
            native_owner_hold=held is not None and any(n['owner'] == n['global'] and not n['frozen']
                and n['epoch'] == states[held]['epoch'] and n['held'] == 1 for n in relevant),
            native_fast_spin_aim=bool(aimed), native_target_mine=bool(matched_mines),
            health_sequence=bool(states) and states[0]['boss_health'] == 1
                and all(s['boss_health'] in (0, 1) for s in states)
                and all(a['boss_health'] >= b['boss_health'] for a, b in zip(states, states[1:])),
            completion_agrees=end.get('reason') == 'native_spin_release_mine_damage'
                and 0xE00 <= end.get('max_spin', 0) <= 0x1000
                and all(end.get(k) == 1 for k in ('release', 'native_throw', 'mine', 'damage')),
        )
    public_end = {key: value for key, value in end.items() if key != '_line'}
    return {'passed': all(checks.values()), 'checks': checks, 'acceptance': public_end,
            'state_count': len(states)}
