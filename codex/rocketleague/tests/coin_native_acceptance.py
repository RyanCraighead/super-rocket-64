"""Strict opt-in two-peer native loot/credit evidence; no gameplay fabrication."""
import hashlib
import json
import math
from pathlib import Path


def finite(value):
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, dict):
        return all(finite(v) for v in value.values())
    if isinstance(value, list):
        return all(finite(v) for v in value)
    return True


def records(text, name):
    prefix = 'ROCKET_COIN_QA_' + name + ' '
    rows = [json.loads(line[len(prefix):]) for line in text.splitlines() if line.startswith(prefix)]
    if any(not isinstance(row, dict) or not finite(row) for row in rows):
        raise ValueError(name + ': nonfinite or nonobject record')
    return rows


def coin_key(row):
    return row['sync'], row['parent'], row['ordinal']


def event_key(row):
    return (row['collector'], row['authority'], row['request'], row['epoch'],
            row['area_sequence'], row['authority_sequence'], *coin_key(row))


def analyze(logs, driver, directory=None):
    checks, errors, observed, captures = {}, [], {}, []
    try:
        if set(logs) != {'host', 'client'} or driver not in logs:
            raise ValueError('Exactly the host and client logs and one driver are required')
        parsed = {role: {name: records(text, name) for name in
                  ('BEGIN', 'END', 'PEER', 'ACCOUNT', 'LOOT', 'EVENT', 'CREDIT', 'CAPTURE')}
                  for role, text in logs.items()}
        own = parsed[driver]
        other_role = 'client' if driver == 'host' else 'host'
        other = parsed[other_role]
        checks['one_native_followup'] = len(own['BEGIN']) == len(own['END']) == 1 and not other['BEGIN'] and not other['END']
        if not checks['one_native_followup']:
            raise ValueError('One driver follow-up begin/end is required')
        begin, end = own['BEGIN'][0], own['END'][0]
        checks['bounded_complete'] = (end['reason'] == 'reward_collected' and
            2200 <= end['ms'] - begin['ms'] <= 26000 and end['timer'] > begin['timer'])
        checks['physical_finite_scene'] = (begin['actor_writes'] == 0 and begin['active'] == end['active'] == 1 and
            begin['ordinary'] == 0 and begin['epoch'] == end['epoch'] and
            begin['parent'] == end['parent'] and begin['parent'] > 0 and end['ordinal'] > 0)
        peers = {role: p['PEER'][0] for role, p in parsed.items() if len(p['PEER']) == 1}
        checks['both_peers_admitted'] = len(peers) == 2 and {p['global'] for p in peers.values()} == {0, 1} and all(
            p['actor_writes'] == 0 and p['level'] == 9 and p['area'] == 1 and p['parent'] == begin['parent']
            for p in peers.values())
        if not checks['both_peers_admitted']:
            raise ValueError('Both matching native area/parent peer admissions are required')
        collector = peers[driver]['global']
        checks['driver_identity'] = begin['global'] == collector
        checks['one_local_credit_only'] = len(own['CREDIT']) == 1 and not other['CREDIT']
        if not checks['one_local_credit_only']:
            raise ValueError('Exactly one driver credit and zero observer credit applications required')
        credit = own['CREDIT'][0]
        expected_key = (credit['sync'], begin['parent'], end['ordinal'])
        checks['credit_source_and_identity'] = (credit['global'] == credit['collector'] == collector and
            credit['authority'] == 0 and credit['epoch'] == begin['epoch'] and credit['request'] > 0 and
            coin_key(credit) == expected_key and credit['area_sequence'] == peers[driver]['area_sequence'] and
            credit['authority_sequence'] == peers['host']['area_sequence'])
        checks['exact_five_point_credit'] = (credit['active'] == credit['result'] == 1 and credit['ordinary'] == 0 and
            credit['value'] == 1 and 0 <= credit['before'] < 95 and
            abs(credit['after'] - credit['before'] - 5) <= .001)
        checks['one_application_counter'] = (credit['serial'] == begin['applications'] + 1 == end['applications'] and
            begin['applications'] == peers[driver]['applications'])
        checks['settled_balance'] = (end['timer'] - credit['timer'] >= 60 and
            abs(end['fuel'] - credit['after']) <= .001 and end['coins'] == begin['coins'] + credit['value'])
        expected_event = event_key(credit)
        stage_rows = lambda role, stage: [r for r in parsed[role]['EVENT'] if r['stage'] == stage]
        pickup = stage_rows(driver, 'pickup')
        claims = stage_rows(driver, 'claim_send')
        accepted = stage_rows(other_role, 'claim_accept')
        checks['native_pickup_and_peer_consumption'] = all(len(rows) == 1 and event_key(rows[0]) == expected_event
            for rows in (pickup, claims, accepted)) and pickup[0]['active'] == 1 and not stage_rows(other_role, 'pickup') and \
            pickup[0]['global'] == claims[0]['global'] == collector and accepted[0]['global'] == peers[other_role]['global']
        if driver == 'client':
            sent, received = stage_rows('host', 'grant_send'), stage_rows('client', 'grant_received')
            checks['authoritative_grant'] = (len(sent) == 1 and bool(received) and
                all(event_key(r) == expected_event for r in sent + received) and
                all(r['global'] == 0 for r in sent) and credit['timer'] >= received[0]['timer'])
        else:
            checks['authoritative_grant'] = not stage_rows('host', 'grant_received') and not stage_rows('client', 'grant_send')
        for role, p in parsed.items():
            peer = peers[role]
            loot, account = p['LOOT'], p['ACCOUNT']
            checks[role + '_one_native_reward'] = (bool(loot) and {coin_key(r) for r in loot} == {expected_key} and
                all(r['global'] == peer['global'] and r['value'] == 1 for r in loot) and any(r['tangible'] for r in loot))
            expected_applications = peer['applications'] + int(role == driver)
            checks[role + '_accounting_observed'] = (len(account) >= 3 and
                all(r['global'] == peer['global'] and r['parent'] == begin['parent'] and 0 <= r['live'] <= 1 for r in account) and
                all(b['timer'] > a['timer'] and b['applications'] >= a['applications'] for a, b in zip(account, account[1:])) and
                account[-1]['applications'] == expected_applications and account[-1]['coins'] == peer['coins'] + 1 and
                account[-1]['live'] == 0)
            if role != driver:
                checks['observer_zero_applications_even_at_full_tank'] = bool(account) and all(r['applications'] == peer['applications'] for r in account)
                checks['observer_after_pickup_interval'] = bool(account and accepted) and account[-1]['timer']-accepted[0]['timer'] >= 30
            observed[role] = dict(reward_keys=sorted({coin_key(r) for r in loot}),
                credit_applications=len(p['CREDIT']), final_applications=account[-1]['applications'] if account else None,
                final_coins=account[-1]['coins'] if account else None)
        checks['two_pickup_captures'] = len(own['CAPTURE']) == 2 and {r['phase'] for r in own['CAPTURE']} == {'before', 'after'}
        if checks['two_pickup_captures']:
            before, after = sorted(own['CAPTURE'], key=lambda r: r['timer'])
            checks['capture_accounting_interval'] = (before['phase'] == 'before' and after['phase'] == 'after' and
                before['timer'] < credit['timer'] <= after['timer'] == end['timer'] and
                all(r['parent'] == begin['parent'] and r['ordinal'] == end['ordinal'] for r in (before, after)) and
                abs(before['fuel'] - credit['before']) <= .001 and abs(after['fuel'] - credit['after']) <= .001)
            if directory is not None:
                for row in (before, after):
                    file = Path(directory) / f"frame-{row['ms']:05d}.ppm"
                    data = file.read_bytes()
                    if not data.startswith(b'P6\n'):
                        raise ValueError('Missing native PPM header: ' + file.name)
                    captures.append(dict(phase=row['phase'], file=file.name, sha256=hashlib.sha256(data).hexdigest()))
        else:
            checks['capture_accounting_interval'] = False
        checks['telemetry_well_formed'] = True
    except (KeyError, IndexError, TypeError, ValueError, OSError) as error:
        errors.append(str(error))
        checks['telemetry_well_formed'] = False
    return dict(schema='rocket-native-coin-followup-v1', passed=bool(checks) and all(checks.values()) and not errors,
        checks=checks, observations=observed, errors=errors, screenshots=captures,
        screenshot_files_verified=directory is not None and len(captures) == 2,
        full_feature_acceptance=False, pending=['Human review of the actual pickup/HUD images',
            'Clamp, native red/blue score values, Infinite/Wing/jet coin cases, simultaneous pickup and reconnect are separate gates'])
