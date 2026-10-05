#!/usr/bin/env python3
"""Read QA-only world-route logs; never launch or control a game.

A completed controller route is not acceptance. Verdicts require the native
observations below. Rendering and two-peer agreement always require separate QA.
"""
import argparse
import json
import math
from pathlib import Path

PREFIX = 'ROCKET_WORLD_QA_'
NAMES = {1: 'bob-platform', 2: 'jrb-water', 3: 'sl-ice'}


def finite_tree(value):
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, dict):
        return all(finite_tree(v) for v in value.values())
    if isinstance(value, list):
        return all(finite_tree(v) for v in value)
    return True


def duration(rows, predicate):
    """Only adjacent qualified observations no more than 500 ms apart count."""
    return sum(b['ms'] - a['ms'] for a, b in zip(rows, rows[1:])
               if 0 < b['ms'] - a['ms'] <= 500 and predicate(a) and predicate(b))


def distance(a, b):
    return math.hypot(a[0] - b[0], a[2] - b[2])


def analyze(lines):
    records = {k: [] for k in ('STAGE', 'STATE', 'PHASE', 'GEOMETRY', 'END')}
    errors = []
    for number, line in enumerate(lines, 1):
        if not line.startswith(PREFIX):
            continue
        try:
            kind, body = line[len(PREFIX):].split(' ', 1)
            value = json.loads(body)
            if not finite_tree(value):
                raise ValueError('nonfinite telemetry')
            if kind in records:
                records[kind].append(value)
        except (ValueError, TypeError) as error:
            errors.append(f'line {number}: {error}')
    states, ends, stages = records['STATE'], records['END'], records['STAGE']
    scene = stages[0].get('scene') if stages else None
    checks = {}
    result = {'scenario': NAMES.get(scene, 'unknown'), 'accepted': False,
              'checks': checks, 'observed': {}, 'errors': errors,
              'unverified': ['Native rendering/screenshots', 'Two-peer socket delivery and agreement',
                             'TTC elevator/cog/pendulum', 'Wind/current magnitude and one-step force composition',
                             'Metal or Vanish platform/water interactions', 'Both players loading one platform']}
    try:
        checks['telemetry_parsed'] = not errors
        checks['one_normal_entry'] = len(stages) == 1 and stages[0]['ordinary_entry_warps'] == 1
        checks['bounded_route_completed'] = len(ends) == 1 and ends[0]['reason'] == 'route_observed' and ends[0]['captures'] <= 3
        checks['observations_present'] = len(states) >= 8
        if not states:
            return result
        checks['scene_consistent'] = scene in NAMES and all(r['scene'] == scene and r['level'] == stages[0]['level'] for r in states)
        checks['focus_preserved'] = all(r['focus'] == 1 for r in states)
        checks['live_physics_without_reset'] = len({r['epoch'] for r in states}) == 1 and states[-1]['ticks'] > states[0]['ticks'] and all(
            b['ticks'] >= a['ticks'] and b['ms'] > a['ms'] for a, b in zip(states, states[1:]))
        checks['native_alive'] = all(r['health'] >= 0x100 for r in states)
        checks['finite_tank_range'] = all(0 <= r['fuel'] <= 100.001 for r in states)
        metrics = result['observed']
        metrics.update(samples=len(states), elapsed_ms=states[-1]['ms'] - states[0]['ms'],
                       health_range=[min(r['health'] for r in states), max(r['health'] for r in states)],
                       fuel_range=[min(r['fuel'] for r in states), max(r['fuel'] for r in states)],
                       surface_modes=sorted({r['surface_mode'] for r in states}),
                       route_end=ends[-1]['reason'] if ends else 'missing')
        if scene == 1:
            def support(r, phase):
                return r['phase'] == phase and r['active'] and r['supported'] and r['grounded'] and 2 <= r['wheels'] <= 4 and r['platform_sync'] > 0 and r['platform_load'] >= 1
            near = [r for r in states if support(r, 3)]
            far = [r for r in states if support(r, 5)]
            near_ms = duration(states, lambda r: support(r, 3))
            far_ms = duration(states, lambda r: support(r, 5))
            leave_ms = duration(states, lambda r: r['phase'] == 6 and not r['supported'] and r['platform_load'] < .01)
            near_torque = sum(r['platform_pitch_velocity'] for r in near) / max(1, len(near))
            far_torque = sum(r['platform_pitch_velocity'] for r in far) / max(1, len(far))
            geometry = [r for r in records['GEOMETRY'] if r['scene'] == 1]
            checks['native_bridge_geometry'] = bool(geometry) and geometry[0]['vertices_with_partition_duplicates'] >= 3 and geometry[0]['along'][1] > geometry[0]['along'][0] + 250
            checks['near_end_supported'] = near_ms >= 1250
            checks['far_end_supported'] = far_ms >= 1250
            checks['opposite_native_response'] = abs(near_torque) > 1 and abs(far_torque) > 1 and near_torque * far_torque < 0
            checks['departed_load_cleared'] = leave_ms >= 750
            metrics.update(near_supported_ms=near_ms, far_supported_ms=far_ms, unloaded_ms=leave_ms,
                           near_mean_pitch_velocity=near_torque, far_mean_pitch_velocity=far_torque,
                           maximum_observed_load=max(r['platform_load'] for r in states))
        elif scene == 2:
            def jet(r):
                return r['active'] and r['water_mode'] == 1 and not (r['caps'] & 12)
            water_ms = duration(states, jet)
            coast_ms = duration(states, lambda r: jet(r) and r['phase'] == 2 and abs(r['input'][0]) < .01 and not r['input'][3])
            qualified = [(a, b) for a, b in zip(states, states[1:]) if 0 < b['ms'] - a['ms'] <= 500 and
                         jet(a) and jet(b) and a['phase'] == b['phase'] == 3 and
                         a['boost_mode'] == b['boost_mode'] == 0 and a['input'][3] and b['input'][3] and a['coins'] == b['coins']]
            boost_ms = sum(b['ms'] - a['ms'] for a, b in qualified)
            checks['actual_jet_water'] = water_ms >= 2000
            checks['neutral_jet_interval'] = coast_ms >= 1500
            checks['coin_only_boost_in_jet'] = boost_ms >= 750
            checks['finite_tank_retained'] = bool(qualified) and all(abs(b['fuel'] - a['fuel']) <= .01 for a, b in qualified)
            jet_rows = [r for r in states if jet(r)]
            metrics.update(jet_ms=water_ms, neutral_jet_ms=coast_ms, qualified_jet_boost_ms=boost_ms,
                           water_modes=sorted({r['water_mode'] for r in states}),
                           jet_height_range=[min(r['pos'][1] for r in jet_rows), max(r['pos'][1] for r in jet_rows)] if jet_rows else None)
        elif scene == 3:
            def ice(r):
                return r['active'] and r['grounded'] and r['wheels'] >= 2 and r['floor'] == 0x2e and (r['material'] & 2) and r['surface_mode'] == 1
            coast = [r for r in states if ice(r) and r['phase'] == 2 and abs(r['input'][0]) < .01 and not r['input'][3]]
            coast_ms = duration(states, lambda r: r in coast)
            brake_ms = duration(states, lambda r: ice(r) and r['phase'] == 3 and r['input'][0] < -.2)
            checks['native_ice_geometry'] = any(r['scene'] == 3 and r['native_type'] == 0x2e for r in records['GEOMETRY'])
            checks['native_surface_coast_interval'] = coast_ms >= 1250
            checks['real_coasting_motion'] = len(coast) >= 2 and distance(coast[0]['pos'], coast[-1]['pos']) > 25
            checks['brake_reached_ice'] = brake_ms >= 500
            metrics.update(ice_coast_ms=coast_ms, ice_brake_ms=brake_ms,
                           coast_distance=distance(coast[0]['pos'], coast[-1]['pos']) if len(coast) >= 2 else 0)
        result['accepted'] = bool(checks) and all(checks.values())
    except (KeyError, TypeError, ValueError, IndexError) as error:
        errors.append(f'missing/invalid required telemetry: {error}')
        checks['required_fields_valid'] = False
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    result = analyze(args.log.read_text(encoding='utf-8', errors='replace').splitlines())
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0 if result['accepted'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
