"""Read-only acceptance of native QA telemetry; never drives or changes the game."""
import json
import math
import re


def analyze(log, scenario, *, contact_evidence=None):
    """Read driver behavior; optional attributed contacts use source time.

    Network callers supply validated, source-ordered ``impacts`` and
    ``contacts`` lists of string-valued records. Local frame counters remain
    the default for the original offline fixtures.
    """
    try:
        if scenario not in ('bob', 'bob-recovery', 'bowser'):
            raise ValueError('Unknown scenario')
        return _analyze(log, scenario, contact_evidence)
    except (KeyError, TypeError, ValueError, OverflowError):
        return {'passed': False, 'checks': {'well_formed_telemetry': False},
                'acceptance': {}, 'impact_count': 0, 'native_throw_count': 0}


def _analyze(log, scenario, contact_evidence=None):
    def fields(marker):
        return [dict(re.findall(r'(\w+)=([^\s]+)', line))
                for line in log.splitlines() if line.startswith(marker + ' ')]

    ends = fields('ROCKET_BOSS_QA_END')
    end = {k: int(v) for k, v in ends[-1].items()} if ends else {}
    impacts = fields('ROCKET_BOSS_IMPACT') if contact_evidence is None else contact_evidence['impacts']
    contacts = fields('ROCKET_BOSS_CONTACT') if contact_evidence is None else contact_evidence['contacts']
    actions = fields('ROCKET_BOSS_QA_ACTION')
    guards = fields('ROCKET_BOSS_QA_GUARD')
    rewards = fields('ROCKET_BOSS_QA_REWARD')
    states = [json.loads(line.split(' ', 1)[1]) for line in log.splitlines()
              if line.startswith('ROCKET_BOSS_QA_STATE ')]
    for state in states:
        if (not isinstance(state, dict) or type(state.get('ms')) is not int or state['ms'] < 0
                or state.get('active') not in (0, 1) or type(state.get('boss_action')) is not int):
            raise ValueError('Malformed native state')
        for key in ('pos', 'velocity', 'boss_pos'):
            values = state.get(key)
            if not isinstance(values, list) or len(values) != 3 or not all(
                    type(v) in (int, float) and math.isfinite(v) for v in values):
                raise ValueError('Malformed native vector')
    recovery = scenario == 'bob-recovery'
    bowser = scenario == 'bowser'
    count = 1 if bowser or recovery else 3
    kind = '1' if bowser else '0'
    throws = [a for a in actions if a.get('action') == ('1' if bowser else '4')]
    checks = {
        'finished': len(ends) == 1 and end.get('complete') == 1,
        'exact_impact_count': len(impacts) == count and all(i.get('kind') == kind for i in impacts),
        'fast_impacts': bool(impacts) and all(math.isfinite(float(i['speed'])) and float(i['speed']) >= 1800 for i in impacts),
        'one_throw_per_impact': len(throws) == count and end.get('throws') == count,
    }
    if contact_evidence is None:
        checks['distinct_impact_frames'] = len({i.get('frame') for i in impacts}) == count
    else:
        checks['distinct_impact_sources'] = len({(i['source'], i['source_epoch'], i['source_tick']) for i in impacts}) == count
    if recovery:
        sequence = [(int(a['action']), int(a['health'])) for a in actions]
        try:
            launch = sequence.index((4, 3))
            returned = sequence.index((5, 3), launch + 1)
            resumed = sequence.index((2, 3), returned + 1)
        except ValueError:
            resumed = None
        resumed_ms = int(actions[resumed]['ms']) if resumed is not None else None
        checks.update(
            off_hill_return_and_dialog=resumed is not None,
            health_preserved=bool(actions) and all(a.get('health') == '3' for a in actions)
                and end.get('health') == 3 and end.get('damage') == 0,
            recovery_recorded=end.get('recovery') == 1 and end.get('recovered') == 1,
            control_resumed=resumed_ms is not None and any(s['ms'] >= resumed_ms + 500
                and s['active'] == 1 and s['boss_action'] == 2 for s in states),
        )
    else:
        checks['native_reward'] = len(rewards) == 1 and rewards[0].get('kind') == ('key' if bowser else 'star')
        checks['native_damage'] = end.get('health') == 0 and end.get('damage') == 1
        checks['launch_health_sequence'] = [int(a['health']) for a in throws] == ([1] if bowser else [3, 2, 1])
        checks['native_defeat'] = any(a.get('action') == ('4' if bowser else '8') and a.get('health') == '0' for a in actions)
    if bowser:
        checks['rear_entry'] = bool(impacts) and all(math.isfinite(float(i.get('rear_cos', 'nan')))
            and -1.0001 <= float(i['rear_cos']) <= -.5 for i in impacts)
    if scenario == 'bob':
        def before_first_impact(contact):
            if not impacts:
                return False
            first = impacts[0]
            if contact_evidence is None:
                return int(contact['frame']) < int(first['frame'])
            if contact['source'] != first['source']:
                return False
            contact_epoch, first_epoch = int(contact['source_epoch']), int(first['source_epoch'])
            if contact_epoch == first_epoch:
                return int(contact['source_tick']) < int(first['source_tick'])
            return 0 < ((first_epoch - contact_epoch) & 0xFFFFFFFF) < 0x80000000
        checks['slow_contact_rejected'] = any(c.get('kind') == '0' and c.get('accepted') == '0'
            and c.get('eligible') == '1' and c.get('action') == '2' and c.get('health') == '3'
            and 0 < float(c['speed']) < 1800 and before_first_impact(c) for c in contacts)
        held = [g for g in guards if g.get('paused') == '1' and g.get('boost') == '1'
                and float(g.get('throttle', 0)) > .9]
        paused_ms = {int(g['ms']) for g in held}
        paused_states = [s for s in states if s['ms'] in paused_ms]
        checks['pause_blocks_held_input'] = (len(held) >= 5
            and int(held[-1]['ms']) - int(held[0]['ms']) >= 500
            and len({g['ticks'] for g in held}) == 1 and int(held[0]['ticks']) > 0
            and all(g['boss_health'] == '3' for g in held)
            and len(paused_states) == len(held)
            and all(sum((s['pos'][i] - paused_states[0]['pos'][i]) ** 2 for i in range(3)) < .01
                    for s in paused_states))
    return {'passed': all(checks.values()), 'checks': checks, 'acceptance': end,
            'impact_count': len(impacts), 'native_throw_count': len(throws)}
