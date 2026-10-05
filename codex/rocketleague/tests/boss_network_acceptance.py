"""Attribute native car/boss contacts across process logs without mixing clocks.

This reads evidence only. The caller separately validates process exit status,
peer delivery/rewards, focus, captures, and requested reconnect coverage.
Missing attribution is a failure, including older otherwise successful runs.
"""
import json
import math
import re
from boss_acceptance import analyze as analyze_gameplay

U32 = 0xFFFFFFFF


def _fields(payload):
    result = {}
    for item in payload.split():
        match = re.fullmatch(r'([a-z_]+)=([^\s=]+)', item)
        if not match or match[1] in result:
            raise ValueError('Malformed or duplicate field')
        result[match[1]] = match[2]
    return result


def _uint(fields, key, maximum=U32, minimum=0):
    value = fields[key]
    if not isinstance(value, str) or not re.fullmatch(r'0|[1-9][0-9]*', value):
        raise ValueError('Invalid unsigned field: ' + key)
    number = int(value)
    if not minimum <= number <= maximum:
        raise ValueError('Out-of-range field: ' + key)
    return number


def _float(fields, key):
    value = float(fields[key])
    if not math.isfinite(value):
        raise ValueError('Nonfinite field: ' + key)
    return value


def _json(payload):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('Duplicate JSON field')
            result[key] = value
        return result
    def constant(_):
        raise ValueError('Nonfinite JSON value')
    value = json.loads(payload, object_pairs_hook=pairs, parse_constant=constant)
    def finite(item):
        if isinstance(item, dict):
            for child in item.values():
                finite(child)
        elif isinstance(item, list):
            for child in item:
                finite(child)
        elif isinstance(item, float) and not math.isfinite(item):
            raise ValueError('Nonfinite JSON value')
    if not isinstance(value, dict):
        raise ValueError('Expected JSON object')
    finite(value)
    return value


def _native(payload):
    row = _json(payload)
    for key in ('frame', 'global', 'owner', 'epoch', 'revision', 'level', 'kind',
                'health', 'action', 'sub', 'held', 'rewards', 'frozen'):
        if type(row[key]) is not int or not 0 <= row[key] <= U32:
            raise ValueError('Invalid native integer: ' + key)
    if row['global'] >= 255 or row['owner'] > 255 or row['frozen'] not in (0, 1):
        raise ValueError('Invalid native identity')
    if not isinstance(row['pos'], list) or len(row['pos']) != 3 or any(
            type(v) not in (int, float) or not math.isfinite(v) for v in row['pos']):
        raise ValueError('Invalid native position')
    return row


def _events(log, process):
    contacts, impacts, natives = [], [], []
    for line_number, line in enumerate(log.splitlines()):
        marker, separator, payload = line.partition(' ')
        if marker == 'BOSS_NET_NATIVE':
            if not separator:
                raise ValueError('Truncated native record')
            natives.append(_native(payload))
        elif marker in ('ROCKET_BOSS_CONTACT', 'ROCKET_BOSS_IMPACT'):
            if not separator:
                raise ValueError('Truncated contact record')
            fields = _fields(payload)
            for key in ('frame', 'health'):
                _uint(fields, key)
            _uint(fields, 'kind', 1)
            for key in ('source', 'authority'):
                _uint(fields, key, 254)
            for key in ('authority_epoch', 'source_epoch'):
                _uint(fields, key, U32, 1)
            _uint(fields, 'source_tick', 0xFFFFFFFFFFFFFFFF)
            _float(fields, 'speed')
            if not -1.0001 <= _float(fields, 'rear_cos') <= 1.0001:
                raise ValueError('Invalid rear cosine')
            if marker == 'ROCKET_BOSS_CONTACT':
                _uint(fields, 'accepted', 1)
                _uint(fields, 'eligible', 1)
                _uint(fields, 'action', 20)
            fields['_process'] = process
            fields['_line'] = line_number
            (contacts if marker == 'ROCKET_BOSS_CONTACT' else impacts).append(fields)
        elif marker == 'ROCKET_BOSS_QA_STATE':
            _json(payload)  # Reject duplicate keys and nonfinite values before the legacy reader.
        elif marker in ('ROCKET_BOSS_QA_ACTION', 'ROCKET_BOSS_QA_GUARD',
                        'ROCKET_BOSS_QA_REWARD', 'ROCKET_BOSS_QA_END'):
            fields = _fields(payload)
            for key in fields:
                if marker == 'ROCKET_BOSS_QA_REWARD' and key == 'kind':
                    if fields[key] not in ('star', 'key'):
                        raise ValueError('Unknown reward kind')
                elif key in ('pos', 'velocity'):
                    values = fields[key].split(',')
                    if len(values) != (3 if key == 'pos' else 2) or any(not math.isfinite(float(v)) for v in values):
                        raise ValueError('Malformed native action/reward vector')
                else:
                    _float(fields, key)
    return contacts, impacts, natives


def _identity(event):
    return tuple(int(event[key]) for key in ('source', 'source_epoch', 'source_tick'))


def _ordered(impacts):
    if not impacts:
        return []
    epochs = {int(event['source_epoch']) for event in impacts}
    first = [epoch for epoch in epochs if all(((other - epoch) & U32) < 0x80000000 for other in epochs)]
    if len(first) != 1:
        raise ValueError('Ambiguous source epoch order')
    return sorted(impacts, key=lambda event: (((int(event['source_epoch']) - first[0]) & U32), int(event['source_tick'])))


def analyze(driver_log, peer_logs, scenario, driver_global):
    """Combine each supplied process log once; never silently deduplicate events."""
    try:
        if scenario not in ('bob', 'bob-recovery', 'bowser') or type(driver_global) is not int or not 0 <= driver_global < 255:
            raise ValueError('Invalid scenario or driver identity')
        if not isinstance(driver_log, str) or not isinstance(peer_logs, list) or any(not isinstance(log, str) for log in peer_logs):
            raise ValueError('Expected one driver log and a list of peer logs')
        return _analyze(driver_log, peer_logs, scenario, driver_global)
    except (KeyError, TypeError, ValueError, OverflowError) as error:
        return {'passed': False, 'checks': {'well_formed_network_telemetry': False},
                'acceptance': {}, 'impact_count': 0, 'native_throw_count': 0,
                'authority_evidence': [], 'error': str(error)}


def _analyze(driver_log, peer_logs, scenario, driver_global):
    contacts, impacts, process_native = [], [], []
    for process, log in enumerate([driver_log] + peer_logs):
        local_contacts, local_impacts, native = _events(log, process)
        contacts.extend(local_contacts)
        impacts.extend(local_impacts)
        process_native.append(native)
    impacts = _ordered(impacts)
    expected_kind = 1 if scenario == 'bowser' else 0
    expected_level = 30 if scenario == 'bowser' else 9
    identities = [{n['global'] for n in rows} for rows in process_native]
    emitting = {e['_process'] for e in contacts + impacts}
    evidence, matching = [], True
    for event in contacts + impacts:
        process = event['_process']
        authority = int(event['authority'])
        epoch = int(event['authority_epoch'])
        frame = int(event['frame'])
        match = identities[process] == {authority} and any(
            n['global'] == n['owner'] == authority and n['epoch'] == epoch and not n['frozen']
            and n['kind'] == int(event['kind']) + 1 and n['level'] == expected_level
            and abs(n['frame'] - frame) <= 3 for n in process_native[process])
        matching = matching and match
        evidence.append(dict(process=process, line=event['_line'] + 1, authority=authority,
                             authority_epoch=epoch, frame=frame, matched=bool(match),
                             source=int(event['source']), source_epoch=int(event['source_epoch']),
                             source_tick=int(event['source_tick'])))
    # CONTACT followed by its IMPACT is expected. Repeated CONTACT/IMPACT lines
    # or attribution of one source sample to multiple simulators are failures.
    distinct_impacts = len({_identity(e) for e in impacts}) == len(impacts)
    distinct_contacts = len({(_identity(e), e['_process']) for e in contacts}) == len(contacts)
    sample_emitters = {}
    for event in contacts + impacts:
        sample_emitters.setdefault(_identity(event), set()).add(event['_process'])
    linked = all(any(_identity(c) == _identity(i) and c['_process'] == i['_process']
                    and c['frame'] == i['frame'] and c['authority_epoch'] == i['authority_epoch']
                    and c['accepted'] == c['eligible'] == '1' and c['_line'] < i['_line']
                    for c in contacts) for i in impacts)
    attributed = dict(well_formed_network_telemetry=True,
        driver_global_matches=identities[0] == {driver_global},
        emitting_processes_identified=all(len(identities[p]) == 1 for p in emitting),
        peer_processes_distinct_from_driver=all(not ids or (len(ids) == 1 and driver_global not in ids) for ids in identities[1:]),
        requested_boss_only=all(int(e['kind']) == expected_kind for e in contacts + impacts),
        impacts_from_driver=bool(impacts) and all(int(i['source']) == driver_global for i in impacts),
        authority_matches_native=bool(impacts) and matching,
        distinct_source_impacts=distinct_impacts, distinct_contact_observations=distinct_contacts,
        no_source_sample_on_multiple_simulators=all(len(emitters) == 1 for emitters in sample_emitters.values()),
        impacts_have_accepted_contact=bool(impacts) and linked,
        impact_health_sequence=[int(i['health']) for i in impacts] == ([1] if scenario == 'bowser' else [3] if scenario == 'bob-recovery' else [3, 2, 1]))
    # Pass the original records, not a rewritten log or synthetic local frames.
    gameplay = analyze_gameplay(driver_log, scenario, contact_evidence=dict(contacts=contacts, impacts=impacts))
    gameplay['checks'].update(attributed)
    gameplay['passed'] = all(gameplay['checks'].values())
    gameplay['authority_evidence'] = evidence
    gameplay['contact_count'] = len(contacts)
    return gameplay
