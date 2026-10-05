"""Check that the native evidence reader fails incomplete or contradictory runs.

These log fixtures test the reader only, not gameplay.
"""
import unittest
from boss_acceptance import analyze

VICTORY = '''ROCKET_BOSS_QA_ACTION ms=100 action=14 health=1
ROCKET_BOSS_IMPACT kind=1 frame=10 speed=2100 health=1 rear_cos=-0.9
ROCKET_BOSS_QA_ACTION ms=200 action=1 health=1
ROCKET_BOSS_QA_ACTION ms=1500 action=4 health=0
ROCKET_BOSS_QA_REWARD kind=key ms=9000
ROCKET_BOSS_QA_END thrown=1 damage=1 throws=1 complete=1 recovery=0 recovered=0 health=0
'''


class EvidenceTests(unittest.TestCase):
    def test_complete_record(self):
        self.assertTrue(analyze(VICTORY, 'bowser')['passed'])

    def test_damage_without_reward_fails(self):
        self.assertFalse(analyze(VICTORY.replace('ROCKET_BOSS_QA_REWARD kind=key ms=9000\n', ''), 'bowser')['passed'])

    def test_duplicate_impact_fails(self):
        duplicate = 'ROCKET_BOSS_IMPACT kind=1 frame=11 speed=2100 health=1 rear_cos=-0.9\n'
        self.assertFalse(analyze(VICTORY + duplicate, 'bowser')['passed'])

    def test_wrong_side_and_nonfinite_speed_fail(self):
        for old, new in [('rear_cos=-0.9', 'rear_cos=0.9'), ('speed=2100', 'speed=nan'), ('speed=2100', 'speed=inf')]:
            self.assertFalse(analyze(VICTORY.replace(old, new), 'bowser')['passed'])

    def test_truncated_or_unknown_telemetry_fails(self):
        for log, scenario in [('', 'bowser'), (VICTORY + 'ROCKET_BOSS_QA_STATE {', 'bowser'),
                (VICTORY + 'ROCKET_BOSS_QA_STATE []', 'bowser'),
                (VICTORY + 'ROCKET_BOSS_QA_STATE {"ms":1,"active":1,"boss_action":1,"pos":[]}', 'bowser'),
                (VICTORY, 'unknown')]:
            self.assertFalse(analyze(log, scenario)['passed'])

    def test_recovery_is_not_damage(self):
        log = '''ROCKET_BOSS_QA_ACTION ms=10 action=2 health=3
ROCKET_BOSS_IMPACT kind=0 frame=20 speed=2000 health=3
ROCKET_BOSS_QA_ACTION ms=100 action=4 health=3
ROCKET_BOSS_QA_ACTION ms=1000 action=5 health=3
ROCKET_BOSS_QA_ACTION ms=5000 action=2 health=3
ROCKET_BOSS_QA_STATE {"ms":5700,"active":1,"boss_action":2,"pos":[0,0,0],"velocity":[0,0,0],"boss_pos":[0,0,0]}
ROCKET_BOSS_QA_END thrown=1 damage=0 throws=1 complete=1 recovery=1 recovered=1 health=3
'''
        self.assertTrue(analyze(log, 'bob-recovery')['passed'])
        self.assertFalse(analyze(log.replace('damage=0', 'damage=1'), 'bob-recovery')['passed'])


if __name__ == '__main__':
    unittest.main()
