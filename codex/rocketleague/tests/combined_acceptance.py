"""Summarize observed QA-only combined routes; never infer hits from proximity.

This analyzer reports individual native observations and explicit coverage gaps.
It does not label a bounded drive or screenshots as full feature acceptance.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path


def records(text, kind):
    prefix = 'ROCKET_COMBINED_QA_' + kind + ' '
    return [json.loads(line[len(prefix):]) for line in text.splitlines() if line.startswith(prefix)]


def analyze(text, directory=None):
    begin, stage, end = (records(text, name) for name in ('BEGIN', 'STAGE', 'END'))
    states, captures = records(text, 'STATE'), records(text, 'CAPTURE')
    errors = []
    if len(begin) != 1 or len(stage) != 1 or len(end) != 1:
        errors.append('One admitted start, normal entry, and bounded end are required')
    meta = begin[0] if begin else {}
    if not states:
        errors.append('No native states observed')
    if any(e.get('reason') != 'bounded_observation_complete' for e in end):
        errors.append('Run stopped without completing bounded observation')
    if meta.get('actor_writes') != 0 or not meta.get('muted') or not meta.get('default_bindings'):
        errors.append('Driver admission record is missing or incompatible')
    if any(s.get('online') not in (0, 1) or s.get('network_role') not in ('none', 'server', 'client')
           or s.get('cap_managed') not in (0, 1) or (s.get('online') and s.get('network_role') == 'none')
           for s in states):
        errors.append('Native network-role or cap-lease telemetry is missing or incompatible')
    if stage and not stage[0].get('normal_entry'):
        errors.append('No normal native entry warp recorded')
    if not 1 <= len(captures) <= 3:
        errors.append('Expected one to three bounded framebuffer captures')
    for s in states:
        values = s['pos'] + s['vel'] + s['mapped'] + s['target_pos'] + s['target_velocity'] + [s['fuel'], s['speed'], s['up']]
        if not all(math.isfinite(v) for v in values):
            errors.append('Nonfinite native telemetry'); break
    if any(b['timer'] <= a['timer'] or b['ms'] < a['ms'] for a, b in zip(states, states[1:])):
        errors.append('Native frame sequence is not strictly increasing')
    if any(s['level'] != meta.get('target_level') for s in states):
        errors.append('Telemetry includes a different scene')
    frames = []
    if directory is not None:
        directory = Path(directory)
        for c in captures:
            image = directory / f"frame-{c['ms']:05d}.ppm"
            if not image.is_file() or not image.read_bytes().startswith(b'P6\n'):
                errors.append(f'Missing native framebuffer: {image.name}')
            else:
                frames.append(dict(file=image.name, sha256=hashlib.sha256(image.read_bytes()).hexdigest()))
    if errors:
        return dict(schema='rocket-combined-native-observation-v1', run_valid=False,
                    errors=errors, observations={}, pending=['No acceptance from an invalid or incomplete run'],
                    full_feature_acceptance=False)

    scenario = meta['scenario']
    active = [s for s in states if s['active']]
    facts = {
        'normal_entry_observed': True,
        'car_control_observed': bool(active),
        'default_virtual_controller_mapping': any(s['mapped'][0] > .1 and abs(s['mapped'][1]) < .01 for s in active),
        'mapped_boost_observed': any(s['mapped'][3] for s in active),
        'native_coin_gains': sum(max(0, b['coins'] - a['coins']) for a, b in zip(states, states[1:])),
        'full_chassis_overlap_observed': any(s['body_contact'] for s in states),
        'native_presentation_observed': any(s['presentation'] and not s['active'] for s in states),
    }
    # A frame showing contact alone is insufficient. Require an actual native
    # damage/launch/knockback transition and the same selected target identity.
    incoming = bump = supersonic = False
    confirmed_targets = set()
    terminal_followup = terminal_damage = False
    knockback_actions = (meta['horizontal_knockback'], meta['vertical_knockback'])
    for i, after in enumerate(states[1:], 1):
        before = states[i-1]
        # Inspect the next collision pass after actual native knockback. Existing
        # hurt-counter drainage is not a new injury or grounds for client immunity.
        if (before['target_valid'] and after['target_valid'] and
                before['target_sync'] == after['target_sync'] and
                before['target_sync'] in confirmed_targets and
                before['target_action'] in knockback_actions and
                after['target_action'] in knockback_actions and
                after['ms'] - before['ms'] <= 200):
            terminal_followup = True
            if after['interact_target'] and (after['hurt'] > before['hurt'] or
                    (before['hurt'] == 0 and after['health'] < before['health'])):
                terminal_damage = True
        nearby = [s for s in states[max(0, i-8):i+1]
                  if after['ms'] - s['ms'] <= 300 and s['target_valid'] and
                  s['target_sync'] == after['target_sync'] and
                  math.dist(s['pos'], s['target_pos']) < 350]
        eligible = [s for s in nearby if s['active'] and s['target_held'] == 0]
        if after['interact_target'] and eligible and (after['health'] < before['health'] or after['hurt'] > before['hurt']):
            incoming = True
        transitioned = (before['target_valid'] and after['target_valid'] and
                        before['target_sync'] == after['target_sync'] and
                        before['target_action'] != after['target_action'])
        if transitioned and eligible:
            if (scenario == 'bump' and after['target_action'] == meta['bobomb_launched'] and
                    max(s['speed'] for s in eligible) < meta['supersonic'] and
                    after['target_velocity'][0] > 0 and not any(s['mapped'][3] for s in eligible)):
                bump = True
            if (scenario == 'supersonic' and after['target_action'] in knockback_actions and
                    any(s['speed'] >= meta['supersonic'] for s in eligible)):
                supersonic = True
                confirmed_targets.add(after['target_sync'])
    facts.update(native_target_damage=incoming, native_bobomb_launch=bump,
                 supersonic_native_knockback=supersonic,
                 supersonic_terminal_followup_observed=terminal_followup,
                 supersonic_terminal_target_damage=terminal_damage,
                 supersonic_confirmed_target_safe=terminal_followup and not terminal_damage)

    def cap_segment(flag):
        return [s for s in states if s['caps'] & flag and s['cap_timer'] > 0]

    flag = meta.get(scenario + '_flag', 0)
    cap = cap_segment(flag) if flag else []
    online_cap = [s for s in cap if s['online']]
    expired = [s for s in states if cap and s['ms'] > cap[-1]['ms'] and not (s['caps'] & flag) and s['cap_timer'] == 0]
    if flag:
        facts.update(native_course_cap_observed=bool(cap),
                     offline_course_cap_observed=any(not s['online'] for s in cap),
                     online_course_lease_observed=any(s['cap_managed'] for s in online_cap),
                     online_course_lease_consistent=bool(online_cap) and all(s['cap_managed'] for s in online_cap),
                     native_cap_countdown=any(b['cap_timer'] < a['cap_timer'] for a, b in zip(cap, cap[1:])),
                     native_cap_expiry=bool(expired),
                     post_expiry_visual_clear=bool(expired) and all(not (s['visual_caps'] & flag) for s in expired))
    # Consecutive active frames are required. Inactive/default snapshots cannot
    # prove tank restoration, and coin pickups cannot be mistaken for recharge.
    pairs = [(a, b) for a, b in zip(states, states[1:]) if a['active'] and b['active'] and
             b['ticks'] > a['ticks'] and b['ms'] - a['ms'] < 200 and a['coins'] == b['coins']]
    metal_pairs = [(a, b) for a, b in pairs if a['water'] == b['water'] == 2 and
                   a['caps'] & meta['metal_flag'] and b['caps'] & meta['metal_flag'] and
                   a['ordinary_mode'] == b['ordinary_mode'] == 0 and
                   a['effective_mode'] == b['effective_mode'] == 0 and a['mapped'][3] and b['mapped'][3]]
    jet_pairs = [(a, b) for a, b in pairs if a['water'] == b['water'] == 1 and a['mapped'][3] and b['mapped'][3]]
    wing_pairs = [(a, b) for a, b in pairs if a['caps'] & meta['wing_flag'] and b['caps'] & meta['wing_flag'] and
                  a['ordinary_mode'] == b['ordinary_mode'] == 0 and a['effective_mode'] == b['effective_mode'] == 1 and
                  a['mapped'][3] and b['mapped'][3]]
    facts.update(metal_water_observed=any(s['water'] == 2 and s['caps'] & meta['metal_flag'] for s in active),
                 finite_metal_boost_spent=any(b['fuel'] < a['fuel'] - .05 for a, b in metal_pairs),
                 jet_after_metal_expiry=any(s['water'] == 1 and not s['caps'] & meta['metal_flag'] for s in expired) if scenario == 'metal' else False,
                 jet_preserves_finite_tank=len(jet_pairs) >= 8 and all(abs(b['fuel']-a['fuel']) < .001 for a, b in jet_pairs),
                 wing_preserves_finite_tank=len(wing_pairs) >= 8 and all(abs(b['fuel']-a['fuel']) < .001 for a, b in wing_pairs),
                 wing_topper_observed=any(s['topper'] and s['caps'] & meta['wing_flag'] for s in states),
                 presentation_topper_observed=any(s['presentation'] and s['topper'] and s['caps'] & meta['wing_flag'] for s in states))
    facts['coin_fuel_credit_observed'] = any(a['active'] and b['active'] and b['coins'] > a['coins'] and
        a['ordinary_mode'] == b['ordinary_mode'] == 0 and b['fuel'] > a['fuel'] + .1
        for a, b in zip(states, states[1:]))

    required = {
        'incoming': ['native_target_damage'], 'bump': ['native_bobomb_launch'],
        'supersonic': ['supersonic_native_knockback', 'supersonic_confirmed_target_safe'],
        'metal': ['native_course_cap_observed', 'native_cap_countdown', 'metal_water_observed', 'native_cap_expiry', 'jet_after_metal_expiry'],
        'wing': ['native_course_cap_observed', 'wing_preserves_finite_tank', 'wing_topper_observed'],
        'vanish': ['native_course_cap_observed', 'native_cap_countdown', 'native_cap_expiry', 'post_expiry_visual_clear'],
    }.get(scenario, [])
    if flag and (meta.get('online') or any(s['online'] for s in states)):
        required += ['online_course_lease_observed', 'online_course_lease_consistent']
    pending = ['Native cap switch unlock and cap-box pickup progression unless separately observed',
               'Remote host/client replication and reconnect for this scene',
               'Physical controller and remapped binding acceptance',
               'Human review of actual HUD/topper/material screenshots']
    if scenario == 'vanish':
        pending.append('Physical barrier passage and expiration recovery against a real barrier')
    if scenario == 'incoming':
        pending.append('Damage exclusively through a chassis region outside the Mario capsule')
    pending += ['Not observed: ' + key for key in required if not facts.get(key)]
    return dict(schema='rocket-combined-native-observation-v1', scenario=scenario, run_valid=True,
                network_roles_observed=sorted({s['network_role'] for s in states}),
                log_sha256=hashlib.sha256(text.encode()).hexdigest(), observations=facts,
                route_evidence_complete=bool(required) and all(facts.get(key) for key in required),
                full_feature_acceptance=False, pending=pending, samples=len(states),
                screenshots=frames, screenshot_files_verified=directory is not None,
                maximum_speed=max((s['speed'] for s in active), default=0),
                minimum_fuel=min((s['fuel'] for s in active), default=None))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--directory', type=Path, help='Verify actual captured PPM files in the private run directory')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    report = analyze(args.log.read_text(errors='replace'), args.directory)
    encoded = json.dumps(report, indent=2) + '\n'
    if args.out:
        args.out.write_text(encoded)
    print(encoded)
    raise SystemExit(0 if report['run_valid'] else 1)
