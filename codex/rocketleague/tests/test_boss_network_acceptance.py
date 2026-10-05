"""Synthetic reader regressions; no fixture result proves native gameplay."""
import json
import unittest
from boss_network_acceptance import analyze


def native(global_id, owner, frame, epoch=5, kind=2, health=1):
    return 'BOSS_NET_NATIVE ' + json.dumps(dict(frame=frame, **{'global': global_id},
        owner=owner, epoch=epoch, revision=1, level=30 if kind == 2 else 9, kind=kind,
        health=health, action=1 if kind == 2 else 2, sub=0, held=0,
        pos=[0, 600, 0], rewards=0, frozen=0), separators=(',', ':'))


def contact(source=1, authority=0, frame=10, epoch=5, source_epoch=2, tick=20,
            kind=1, health=1, speed=2100, accepted=1):
    return (f'ROCKET_BOSS_CONTACT kind={kind} frame={frame} accepted={accepted} eligible=1 '
            f'speed={speed} action=2 health={health} rear_cos=-0.9 '
            f'source={source} authority={authority} authority_epoch={epoch} '
            f'source_epoch={source_epoch} source_tick={tick}')


def impact(**changes):
    return contact(**changes).replace('ROCKET_BOSS_CONTACT', 'ROCKET_BOSS_IMPACT').replace(
        ' accepted=1 eligible=1', '').replace(' action=2', '')


def attack(authority, frame, epoch=5, source_epoch=2, tick=20, kind=1, health=1, source=1):
    args = dict(source=source, authority=authority, frame=frame, epoch=epoch,
                source_epoch=source_epoch, tick=tick, kind=kind, health=health)
    return '\n'.join((native(authority, authority, frame, epoch, kind + 1, health),
                      contact(**args), impact(**args)))


BOWSER_GAMEPLAY = '''ROCKET_BOSS_QA_ACTION ms=100 action=14 health=1 pos=4.00,600.00,-923.00 velocity=0.00,0.00
ROCKET_BOSS_QA_ACTION ms=200 action=1 health=1 pos=4.00,600.00,-923.00 velocity=220.75,91.44
ROCKET_BOSS_QA_ACTION ms=1500 action=4 health=0 pos=0.00,600.00,2949.00 velocity=0.00,0.00
ROCKET_BOSS_QA_REWARD kind=key ms=9000 pos=0.00,600.00,2949.00
ROCKET_BOSS_QA_END thrown=1 damage=1 throws=1 complete=1 recovery=0 recovered=0 health=0
'''


def bowser():
    return native(1, 0, 4000) + '\n' + BOWSER_GAMEPLAY, [attack(0, 10)]


def bob():
    lines = [native(1, 1, 900000, epoch=4, kind=1, health=3),
             contact(authority=1, frame=900000, epoch=4, kind=0, health=3, speed=416,
                     accepted=0, tick=10), 'ROCKET_BOSS_QA_ACTION ms=200 action=2 health=3']
    for ms in (1000, 1125, 1250, 1375, 1500):
        lines.append(f'ROCKET_BOSS_QA_GUARD ms={ms} paused=1 boost=1 throttle=1 ticks=128 boss_health=3')
        lines.append('ROCKET_BOSS_QA_STATE ' + json.dumps(dict(ms=ms, active=1, boss_action=2,
                      pos=[0, 600, 0], velocity=[0, 0, 0], boss_pos=[0, 600, 500]), separators=(',', ':')))
    lines += ['ROCKET_BOSS_QA_ACTION ms=2000 action=4 health=3',
              'ROCKET_BOSS_QA_ACTION ms=3000 action=4 health=2',
              'ROCKET_BOSS_QA_ACTION ms=4000 action=4 health=1',
              'ROCKET_BOSS_QA_ACTION ms=8000 action=8 health=0',
              'ROCKET_BOSS_QA_REWARD kind=star ms=9000',
              'ROCKET_BOSS_QA_END thrown=1 damage=1 throws=3 complete=1 recovery=0 recovered=0 health=0']
    peer = '\n'.join(attack(0, frame, tick=tick, kind=0, health=health)
                     for frame, tick, health in ((10, 20, 3), (20, 30, 2), (30, 40, 1)))
    return '\n'.join(lines), [peer]


class BossNetworkEvidenceTests(unittest.TestCase):
    def verdict(self, driver, peers, scenario='bowser', global_id=1):
        return analyze(driver, peers, scenario, global_id)

    def test_remote_attack_is_attributed_to_host_simulator(self):
        result = self.verdict(*bowser())
        self.assertTrue(result['passed'], result)
        self.assertEqual(result['impact_count'], 1)
        self.assertEqual(result['contact_count'], 1)
        self.assertEqual(len(result['authority_evidence']), 2)
        self.assertTrue(all(e['authority'] == 0 and e['source'] == 1 and e['matched']
                            for e in result['authority_evidence']))

    def test_local_driver_and_empty_outside_observer(self):
        driver = attack(1, 10) + '\n' + BOWSER_GAMEPLAY
        self.assertTrue(self.verdict(driver, ['unrelated network diagnostics\n'])['passed'])
        driver = attack(0, 10, source=0) + '\n' + BOWSER_GAMEPLAY
        self.assertTrue(self.verdict(driver, [], global_id=0)['passed'])

    def test_reconnect_process_has_separate_identity_and_clock(self):
        driver, peers = bowser()
        peers.append(native(0, 0, 1))
        self.assertTrue(self.verdict(driver, peers)['passed'])
        peers[-1] = attack(0, 1)
        result = self.verdict(driver, peers)
        self.assertFalse(result['passed'])
        self.assertFalse(result['checks']['distinct_source_impacts'])
        self.assertFalse(result['checks']['no_source_sample_on_multiple_simulators'])

    def test_duplicate_events_are_not_silently_deduplicated(self):
        driver, peers = bowser()
        for added in (impact(), contact()):
            self.assertFalse(self.verdict(driver, [peers[0] + '\n' + added])['passed'])
        self.assertFalse(self.verdict(driver, peers + peers)['passed'])

    def test_contact_and_impact_pair_counts_one_attack(self):
        result = self.verdict(*bowser())
        self.assertTrue(result['checks']['distinct_source_impacts'])
        self.assertTrue(result['checks']['no_source_sample_on_multiple_simulators'])
        self.assertEqual(result['native_throw_count'], 1)

    def test_accepted_contact_must_precede_impact_on_same_emitter(self):
        driver, _ = bowser()
        for stream in (native(0, 0, 10) + '\n' + impact(),
                       '\n'.join((native(0, 0, 10), impact(), contact())),
                       '\n'.join((native(0, 0, 10), contact(accepted=0), impact()))):
            self.assertFalse(self.verdict(driver, [stream])['passed'])

    def test_wrong_attacker_authority_epoch_owner_or_frozen_fails(self):
        driver, peers = bowser()
        for old, new in [('source=1', 'source=2'), ('authority=0', 'authority=1'),
                         ('authority_epoch=5', 'authority_epoch=6'), ('"owner":0', '"owner":1'),
                         ('"frozen":0', '"frozen":1'), ('"global":0', '"global":2')]:
            with self.subTest(change=old):
                self.assertFalse(self.verdict(driver, [peers[0].replace(old, new)])['passed'])
        self.assertFalse(self.verdict(driver, peers, global_id=0)['passed'])

    def test_native_authority_sample_window_is_three_frames(self):
        driver, peers = bowser()
        for delta in (-3, 3):
            self.assertTrue(self.verdict(driver, [peers[0].replace('"frame":10', f'"frame":{10 + delta}')])['passed'])
        for delta in (-4, 4):
            self.assertFalse(self.verdict(driver, [peers[0].replace('"frame":10', f'"frame":{10 + delta}')])['passed'])
        self.assertFalse(self.verdict(driver, ['\n'.join(peers[0].splitlines()[1:])])['passed'])

    def test_bob_preimpact_slow_contact_uses_source_ticks_not_frames(self):
        driver, peers = bob()
        result = self.verdict(driver, peers, 'bob')
        self.assertTrue(result['passed'], result)
        self.assertTrue(result['checks']['slow_contact_rejected'])
        # The driver's local frame can be smaller yet its car sample is later.
        late = driver.replace('900000', '1').replace('source_tick=10', 'source_tick=25')
        result = self.verdict(late, peers, 'bob')
        self.assertFalse(result['passed'])
        self.assertFalse(result['checks']['slow_contact_rejected'])
        other_car = driver.replace('source=1', 'source=2')
        self.assertFalse(self.verdict(other_car, peers, 'bob')['checks']['slow_contact_rejected'])

    def test_recovery_retains_native_health_and_control_requirements(self):
        driver = native(1, 0, 500, kind=1, health=3) + '''
ROCKET_BOSS_QA_ACTION ms=10 action=2 health=3
ROCKET_BOSS_QA_ACTION ms=100 action=4 health=3
ROCKET_BOSS_QA_ACTION ms=1000 action=5 health=3
ROCKET_BOSS_QA_ACTION ms=5000 action=2 health=3
ROCKET_BOSS_QA_STATE {"ms":5700,"active":1,"boss_action":2,"pos":[0,0,0],"velocity":[0,0,0],"boss_pos":[0,0,0]}
ROCKET_BOSS_QA_END thrown=1 damage=0 throws=1 complete=1 recovery=1 recovered=1 health=3
'''
        peers = [attack(0, 10, kind=0, health=3)]
        self.assertTrue(self.verdict(driver, peers, 'bob-recovery')['passed'])
        self.assertFalse(self.verdict(driver.replace('damage=0', 'damage=1'), peers, 'bob-recovery')['passed'])
        self.assertFalse(self.verdict(driver.replace('"ms":5700', '"ms":5100'), peers, 'bob-recovery')['passed'])

    def test_impact_frames_can_coincide_on_different_simulators(self):
        driver, peers = bob()
        # Put the second hit on the driver, whose frame is coincidentally 10.
        second = attack(1, 10, epoch=6, tick=30, kind=0, health=2)
        driver = driver.replace('900000', '1') + '\n' + second
        peer = attack(0, 10, tick=20, kind=0, health=3) + '\n' + attack(0, 20, epoch=7, tick=40, kind=0, health=1)
        result = self.verdict(driver, [peer], 'bob')
        self.assertTrue(result['passed'], result)
        self.assertIn('distinct_impact_sources', result['checks'])
        self.assertNotIn('distinct_impact_frames', result['checks'])

    def test_source_epoch_reset_and_wrap_preserve_causal_order(self):
        driver, peers = bob()
        driver = driver.replace('source_epoch=2', 'source_epoch=4294967295')
        peer = '\n'.join((attack(0, 10, source_epoch=4294967295, tick=20, kind=0, health=3),
                          attack(0, 20, source_epoch=1, tick=1, kind=0, health=2),
                          attack(0, 30, source_epoch=1, tick=2, kind=0, health=1)))
        self.assertTrue(self.verdict(driver, [peer], 'bob')['passed'])
        # Source epochs half a uint32 range apart do not have a reliable order.
        peer = peer.replace('source_epoch=1 ', 'source_epoch=2147483647 ')
        self.assertFalse(self.verdict(driver, [peer], 'bob')['passed'])

    def test_uint64_ticks_are_not_truncated(self):
        driver, _ = bowser()
        self.assertTrue(self.verdict(driver, [attack(0, 10, tick=0x100000001)])['passed'])
        self.assertFalse(self.verdict(driver, [attack(0, 10, tick=0x10000000000000000)])['passed'])

    def test_missing_attribution_is_not_a_retroactive_pass(self):
        driver, peers = bowser()
        for field in ('source', 'authority', 'authority_epoch', 'source_epoch', 'source_tick'):
            lines = []
            for line in peers[0].splitlines():
                lines.append(' '.join(word for word in line.split(' ') if not word.startswith(field + '=')))
            self.assertFalse(self.verdict(driver, ['\n'.join(lines)])['passed'])

    def test_malformed_nonfinite_duplicate_or_mixed_native_identity_fails(self):
        driver, peers = bowser()
        for old, new in [('speed=2100', 'speed=nan'), ('source_tick=20', 'source_tick=-1'),
                         ('source_epoch=2', 'source_epoch=0'), ('authority=0', 'authority=0 authority=0'),
                         ('"pos":[0,600,0]', '"pos":[0,1e999,0]'),
                         ('"global":0', '"global":true'), ('"frame":10', '"frame":10,"frame":10')]:
            self.assertFalse(self.verdict(driver, [peers[0].replace(old, new)])['passed'])
        self.assertFalse(self.verdict(driver, peers + [native(0, 0, 1) + '\n' + native(2, 2, 2)])['passed'])
        for extra in ('BOSS_NET_NATIVE []', 'ROCKET_BOSS_CONTACT', 'ROCKET_BOSS_QA_STATE {'):
            self.assertFalse(self.verdict(driver + '\n' + extra, peers)['passed'])

    def test_gameplay_requirements_still_apply(self):
        driver, peers = bowser()
        self.assertFalse(self.verdict(driver.replace('ROCKET_BOSS_QA_REWARD kind=key ms=9000 pos=0.00,600.00,2949.00\n', ''), peers)['passed'])
        self.assertFalse(self.verdict(driver.replace('complete=1', 'complete=0'), peers)['passed'])
        self.assertFalse(self.verdict(driver, [peers[0].replace('speed=2100', 'speed=100')])['passed'])
        self.assertFalse(self.verdict(driver, [peers[0].replace('rear_cos=-0.9', 'rear_cos=0.9')])['passed'])

    def test_actual_action_and_reward_vectors_are_validated(self):
        driver, peers = bowser()
        self.assertTrue(self.verdict(driver, peers)['passed'])
        for old, new in [('pos=4.00,600.00,-923.00', 'pos=4.00,nan,-923.00'),
                         ('velocity=220.75,91.44', 'velocity=220.75'),
                         ('pos=0.00,600.00,2949.00', 'pos=0.00,600.00,inf')]:
            self.assertFalse(self.verdict(driver.replace(old, new), peers)['passed'])

    def test_invalid_api_inputs_fail_closed(self):
        driver, peers = bowser()
        for d, p, s, g in ((None, peers, 'bowser', 1), (driver, 'peer', 'bowser', 1),
                           (driver, peers, 'unknown', 1), (driver, peers, 'bowser', True)):
            self.assertFalse(analyze(d, p, s, g)['passed'])


if __name__ == '__main__':
    unittest.main()
