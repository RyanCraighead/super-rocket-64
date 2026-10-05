"""Reader fixtures only: passing these is not evidence of native gameplay."""
import json
import unittest
from mario_boss_acceptance import analyze, ACT_GRABBED, ACT_HOLDING_BOWSER, ACT_RELEASING_BOWSER


def record(marker, **data):
    return marker + ' ' + json.dumps(data, separators=(',', ':'))


def state(scenario, ms, **changes):
    data = dict(ms=ms, frame=ms // 33, scenario=scenario, action=0x04000440,
                action_arg=0, health=0x880, pos=[0, 600, 0], yaw=0, spin=0,
                boss_action=2, boss_sub=0, boss_health=3 if scenario == 'bob' else 1,
                boss_pos=[0, 600, 400], held=0, grabbed=0, escape=0, owner=1,
                epoch=4, waypoint=9, blocked=0, pad=[0, 0, 0, 0])
    data.update(changes)
    return record('MARIO_BOSS_QA_STATE', **data)


def native(scenario, **changes):
    data = dict(frame=31, global_=1, owner=1, epoch=4, revision=5,
                level=9 if scenario == 'bob' else 30, kind=1 if scenario == 'bob' else 2,
                health=3 if scenario == 'bob' else 1, action=3 if scenario == 'bob' else 0,
                sub=1, held=0 if scenario == 'bob' else 1, pos=[0, 600, 0], rewards=0, frozen=0)
    data['global'] = data.pop('global_')
    data.update(changes)
    return record('BOSS_NET_NATIVE', **data)


def end(scenario, **changes):
    bowser = scenario == 'bowser'
    data = dict(scenario=scenario, passed=1,
                reason='native_spin_release_mine_damage' if bowser else 'native_grab_button_escape',
                authority=1, grab=1, max_spin=4096 if bowser else 0, release=int(bowser),
                native_throw=int(bowser), mine=int(bowser), damage=int(bowser),
                escape=int(not bowser), max_escape=0 if bowser else 11, ms=4000)
    data.update(changes)
    return record('MARIO_BOSS_QA_END', **data)


def fixture(scenario):
    prefix = [f'MARIO_BOSS_QA_BEGIN scenario={scenario} input=virtual_sdl_only',
              'MARIO_BOSS_QA_STAGE ' + ('level=30 node=-1' if scenario == 'bowser' else 'level=9 node=11'),
              native(scenario)]
    if scenario == 'bob':
        return prefix + [state(scenario, 1000, action=ACT_GRABBED, grabbed=1, boss_action=3, pad=[0, 0, 1, 0]),
                         state(scenario, 1600, escape=11), state(scenario, 2400, escape=11), end(scenario)]
    return prefix + [state(scenario, 1000, action=ACT_HOLDING_BOWSER, held=1, spin=4096),
                     'MARIO_BOSS_QA_RELEASE_REQUEST ms=1100 spin=4096 yaw=0 error=0.1 mine=[0.0,589.0,2949.0]',
                     state(scenario, 1200, action=ACT_RELEASING_BOWSER),
                     state(scenario, 1250, action=ACT_RELEASING_BOWSER, boss_action=1),
                     record('MARIO_BOSS_QA_MINE', ms=2000, pos=[0, 589, 2949], target=[0, 589, 2949]),
                     state(scenario, 2000, boss_action=4, boss_health=0), end(scenario)]


class MarioBossEvidenceTests(unittest.TestCase):
    def verdict(self, lines, scenario='bob'):
        return analyze('\n'.join(lines), scenario)['passed']

    def test_complete_synthetic_records(self):
        for scenario in ('bob', 'bowser'):
            with self.subTest(scenario=scenario):
                self.assertTrue(self.verdict(fixture(scenario), scenario))

    def test_every_required_line_is_required(self):
        for scenario in ('bob', 'bowser'):
            lines = fixture(scenario)
            for index in range(len(lines)):
                # Release and thrown state may legitimately be one snapshot.
                if scenario == 'bowser' and index == 5:
                    continue
                with self.subTest(scenario=scenario, line=index):
                    self.assertFalse(self.verdict(lines[:index] + lines[index + 1:], scenario))

    def test_end_success_alone_is_insufficient(self):
        self.assertFalse(self.verdict([end('bob')]))
        self.assertFalse(self.verdict(fixture('bob')[:3] + [end('bob')]))

    def test_duplicate_completion_or_staging_fails(self):
        lines = fixture('bob')
        for added in (lines[-1], lines[0], lines[1], lines[3]):
            self.assertFalse(self.verdict(lines + [added]))
        self.assertFalse(self.verdict(lines[:2] + [lines[1]] + lines[2:]))

    def test_chronology_and_escape_holdoff(self):
        lines = fixture('bob')
        self.assertFalse(self.verdict(lines[:3] + [lines[4], lines[3]] + lines[5:]))
        self.assertFalse(self.verdict(lines[:5] + [state('bob', 2000, escape=11), lines[-1]]))
        lines = fixture('bowser')
        self.assertFalse(self.verdict(lines[:7] + [lines[8], lines[7]] + lines[9:], 'bowser'))

    def test_native_authority_required(self):
        for scenario in ('bob', 'bowser'):
            lines = fixture(scenario)
            self.assertFalse(self.verdict([line.replace('"owner":1', '"owner":0') for line in lines], scenario))
            self.assertFalse(self.verdict([line.replace('"epoch":4', '"epoch":0') for line in lines], scenario))
            lines[2] = native(scenario, owner=2)
            self.assertFalse(self.verdict(lines, scenario))

    def test_bob_damage_rewards_throw_or_no_escape_input_fail(self):
        lines = fixture('bob')
        for marker, replacement in [('"boss_health":3', '"boss_health":2'),
                                    ('"rewards":0', '"rewards":1'),
                                    ('"escape":11', '"escape":10'),
                                    ('"pad":[0,0,1,0]', '"pad":[0,0,0,0]')]:
            self.assertFalse(self.verdict([line.replace(marker, replacement) for line in lines]))
        self.assertFalse(self.verdict(lines + ['ROCKET_BOSS_QA_REWARD kind=star']))

    def test_bowser_spin_aim_target_and_native_damage_required(self):
        text = '\n'.join(fixture('bowser'))
        for old, new in [('spin=4096', 'spin=100'), ('error=0.1', 'error=1'),
                         ('mine=[0.0,589.0,2949.0]', 'mine=[2949.0,589.0,0.0]'),
                         ('"target":[0,589,2949]', '"target":[0,589,2600]'),
                         ('"boss_health":0', '"boss_health":1'),
                         ('"action_arg":0', '"action_arg":1')]:
            self.assertFalse(analyze(text.replace(old, new), 'bowser')['passed'])

    def test_car_impact_disqualifies_both_scenarios(self):
        for scenario in ('bob', 'bowser'):
            self.assertFalse(self.verdict(fixture(scenario) + ['ROCKET_BOSS_IMPACT kind=1 frame=20'], scenario))

    def test_native_mine_explosion_next_frame_requires_observed_authority(self):
        lines = fixture('bowser')
        prefix = lines[:7] + [state('bowser', 2000, frame=60, boss_action=4, boss_health=0)]
        mine = record('MARIO_BOSS_QA_MINE', ms=2033, pos=[0, 589, 2949], target=[0, 589, 2949])
        observed = state('bowser', 2033, frame=61, boss_action=4, boss_health=0)
        self.assertTrue(self.verdict(prefix + [mine, observed, lines[-1]], 'bowser'))
        self.assertFalse(self.verdict(prefix + [mine, lines[-1]], 'bowser'))
        for changes in ({'frame': 62}, {'owner': 0}, {'epoch': 5}, {'boss_health': 1}, {'boss_action': 1}):
            data = dict(frame=61, boss_action=4, boss_health=0)
            data.update(changes)
            self.assertFalse(self.verdict(prefix + [mine, state('bowser', 2033, **data), lines[-1]], 'bowser'))

    def test_unsampled_spin_threshold_uses_native_release_request(self):
        lines = fixture('bowser')
        lines[3] = state('bowser', 1000, action=ACT_HOLDING_BOWSER, held=1, spin=3500)
        self.assertTrue(self.verdict(lines, 'bowser'))
        lines[4] = lines[4].replace('spin=4096', 'spin=3500')
        self.assertFalse(self.verdict(lines, 'bowser'))

    def test_native_hidden_throw_wrong_epoch_or_player_fails(self):
        lines = fixture('bob')
        self.assertFalse(self.verdict(lines[:3] + [native('bob', action=4)] + lines[3:]))
        for replacement in (native('bob', epoch=5), native('bob', frozen=1)):
            self.assertFalse(self.verdict(lines[:2] + [replacement] + lines[3:]))
        self.assertFalse(self.verdict(lines[:3] + [native('bob', **{'global': 2})] + lines[3:]))

    def test_authority_cannot_change_during_hold_or_escape(self):
        for scenario, index in (('bob', 4), ('bowser', 6)):
            lines = fixture(scenario)
            lines[index] = lines[index].replace('"epoch":4', '"epoch":5')
            self.assertFalse(self.verdict(lines, scenario))
        lines = fixture('bowser')
        lines.insert(5, lines[4].replace('ms=1100', 'ms=900'))
        self.assertFalse(self.verdict(lines, 'bowser'))

    def test_dead_mario_cannot_pass(self):
        lines = [line.replace('"health":2176', '"health":0') for line in fixture('bob')]
        self.assertFalse(self.verdict(lines))

    def test_malformed_shape_nonfinite_and_duplicate_keys(self):
        lines = fixture('bob')
        for value in ('[]', 'null', '{}', '{', '{"ms":1,"ms":2}'):
            self.assertFalse(self.verdict(lines[:3] + ['MARIO_BOSS_QA_STATE ' + value] + lines[4:]))
        for old, new in [('"ms":1000', '"ms":true'), ('"pos":[0,600,0]', '"pos":null'),
                         ('"pos":[0,600,0]', '"pos":[0,NaN,0]'), ('"spin":0', '"spin":1e999'),
                         ('"held":0', '"held":2'), ('"pad":[0,0,1,0]', '"pad":[2,0,1,0]')]:
            self.assertFalse(self.verdict([line.replace(old, new) for line in lines]))
        bowser = '\n'.join(fixture('bowser'))
        self.assertFalse(analyze(bowser.replace('error=0.1', 'error=nan'), 'bowser')['passed'])

    def test_failed_rejected_mixed_or_input_blocked_run(self):
        lines = fixture('bob')
        self.assertFalse(self.verdict(lines[:-1] + [end('bob', passed=0)]))
        self.assertFalse(self.verdict(lines + ['MARIO_BOSS_QA_REJECT invalid_admission_or_pad']))
        self.assertFalse(self.verdict([line.replace('"scenario":"bob"', '"scenario":"bowser"') for line in lines]))
        self.assertFalse(self.verdict([line.replace('"blocked":0', '"blocked":1') for line in lines]))
        self.assertFalse(analyze('', 'unknown')['passed'])
        self.assertFalse(analyze(None, 'bob')['passed'])


if __name__ == '__main__':
    unittest.main()
