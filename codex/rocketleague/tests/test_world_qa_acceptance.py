"""Synthetic evidence-reader tests. These are not native gameplay evidence."""
import copy
import json
import unittest
from analyze_world_qa import analyze


def fixture(scene):
    phases = {1: [1]*4+[3]*8+[4]*4+[5]*8+[6]*6,
              2: [1]*4+[2]*16+[3]*8+[4]*8,
              3: [1]*4+[2]*10+[3]*8}[scene]
    records = [('STAGE', dict(scene=scene, ms=500, level=scene+8, ordinary_entry_warps=1))]
    if scene == 1:
        records.append(('GEOMETRY', dict(scene=1, vertices_with_partition_duplicates=12, along=[-500, 500])))
    if scene == 3:
        records.append(('GEOMETRY', dict(scene=3, native_type=0x2e)))
    for i, phase in enumerate(phases):
        support = scene == 1 and phase in (3, 5)
        throttle = .4 if scene == 2 and phase == 3 else -1 if scene == 3 and phase == 3 else 0
        records.append(('STATE', dict(scene=scene, ms=1000+i*250, frame=i, phase=phase, level=scene+8,
            active=1, focus=1, epoch=7, ticks=4*i, pos=[float(i*30), 0, 0], velocity=[120, 0, 0], grounded=1,
            wheels=4, water_mode=int(scene == 2 and phase >= 2), water_level=200,
            health=0x880, coins=0, caps=0, boost_mode=0, surface_mode=1, fuel=37.5,
            input=[throttle, 0, 0, int(scene == 2 and phase == 3)], floor=0x2e if scene == 3 else 0,
            material=2 if scene == 3 else 0, supported=int(support), platform_sync=11 if scene == 1 else 0,
            platform_load=2 if support else 0, platform_pitch=100 if phase == 3 else -100,
            platform_pitch_velocity=30 if phase == 3 else -30)))
    records.append(('END', dict(scene=scene, ms=1000+len(phases)*250, reason='route_observed', captures=3)))
    return records


def result(records):
    return analyze(['ROCKET_WORLD_QA_'+kind+' '+json.dumps(value) for kind, value in records])


class EvidenceReader(unittest.TestCase):
    def mutate(self, scene, callback):
        data=copy.deepcopy(fixture(scene))
        for kind, value in data:
            callback(kind, value)
        return result(data)

    def test_three_positive_reader_fixtures(self):
        for scene in (1, 2, 3):
            with self.subTest(scene=scene):
                verdict=result(fixture(scene))
                self.assertTrue(verdict['accepted'], verdict)
                self.assertIn('Two-peer socket delivery and agreement', verdict['unverified'])

    def test_route_end_alone_never_accepts(self):
        self.assertFalse(result([fixture(1)[0],fixture(1)[-1]])['accepted'])

    def test_timeout_stays_incomplete_even_with_support(self):
        verdict=self.mutate(1,lambda k,v: v.update(reason='route_timeout') if k=='END' else None)
        self.assertFalse(verdict['accepted'])

    def test_focus_loss_stays_incomplete(self):
        verdict=self.mutate(2,lambda k,v: v.update(reason='focus_lost') if k=='END' else None)
        self.assertFalse(verdict['accepted'])

    def test_hidden_reset_fails(self):
        verdict=self.mutate(1,lambda k,v: v.update(epoch=8) if k=='STATE' and v['phase']==5 else None)
        self.assertFalse(verdict['accepted'])

    def test_missing_far_support_fails(self):
        verdict=self.mutate(1,lambda k,v: v.update(supported=0) if k=='STATE' and v['phase']==5 else None)
        self.assertFalse(verdict['accepted'])

    def test_same_direction_torque_fails(self):
        verdict=self.mutate(1,lambda k,v: v.update(platform_pitch_velocity=30) if k=='STATE' else None)
        self.assertFalse(verdict['accepted'])

    def test_stale_unloaded_weight_fails(self):
        verdict=self.mutate(1,lambda k,v: v.update(platform_load=2) if k=='STATE' and v['phase']==6 else None)
        self.assertFalse(verdict['accepted'])

    def test_sparse_telemetry_does_not_fill_gaps(self):
        verdict=self.mutate(1,lambda k,v: v.update(ms=v['ms']*10) if k=='STATE' else None)
        self.assertFalse(verdict['accepted'])

    def test_infinite_rule_is_not_jet_fuel_evidence(self):
        verdict=self.mutate(2,lambda k,v: v.update(boost_mode=1) if k=='STATE' else None)
        self.assertFalse(verdict['accepted'])

    def test_wing_allowance_is_not_jet_fuel_evidence(self):
        verdict=self.mutate(2,lambda k,v: v.update(caps=8) if k=='STATE' else None)
        self.assertFalse(verdict['accepted'])

    def test_dry_boost_is_not_jet_evidence(self):
        verdict=self.mutate(2,lambda k,v: v.update(water_mode=0) if k=='STATE' and v['phase']==3 else None)
        self.assertFalse(verdict['accepted'])

    def test_fuel_consumption_while_jet_boosting_fails(self):
        verdict=self.mutate(2,lambda k,v: v.update(fuel=37.5-v['ms']/10000) if k=='STATE' and v['phase']==3 else None)
        self.assertFalse(verdict['accepted'])

    def test_coins_do_not_masquerade_as_stable_fuel(self):
        verdict=self.mutate(2,lambda k,v: v.update(coins=v['frame']) if k=='STATE' and v['phase']==3 else None)
        self.assertFalse(verdict['accepted'])

    def test_car_grip_is_not_native_ice_acceptance(self):
        verdict=self.mutate(3,lambda k,v: v.update(surface_mode=0) if k=='STATE' else None)
        self.assertFalse(verdict['accepted'])

    def test_floor_without_tires_is_not_ice_contact(self):
        verdict=self.mutate(3,lambda k,v: v.update(wheels=0) if k=='STATE' else None)
        self.assertFalse(verdict['accepted'])

    def test_native_ice_label_without_motion_fails(self):
        verdict=self.mutate(3,lambda k,v: v.update(pos=[0,0,0]) if k=='STATE' else None)
        self.assertFalse(verdict['accepted'])

    def test_malformed_and_nonfinite_telemetry_fails(self):
        records=fixture(2)
        for kind,value in records:
            if kind=='STATE':
                value['fuel']=float('nan')
                break
        self.assertFalse(result(records)['accepted'])
        self.assertFalse(analyze(['ROCKET_WORLD_QA_STATE malformed'])['accepted'])


if __name__=='__main__':
    unittest.main()
