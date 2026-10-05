"""Synthetic analyzer regressions only; these are not native game evidence."""
import copy
import json
import unittest
from combined_acceptance import analyze


def state(i):
    return dict(ms=2000+i*34,timer=100+i,level=9,active=1,presentation=0,action=0,health=2176,hurt=0,
                invinc=0,caps=0,visual_caps=0,cap_timer=0,online=0,network_role='none',cap_managed=0,
                topper=0,coins=0,fuel=50,water=0,ticks=i*4,
                speed=100,pos=[0,0,0],vel=[0,0,100],up=1,ground=1,waypoint=0,ordinary_mode=0,
                effective_mode=0,mapped=[.5,0,0,0,0],sent=[.5,0,0,0,0],buttons=0,interact_target=0,
                body_contact=1,target_valid=1,target_active=1,target_action=0,target_held=0,target_sync=25,
                target_pos=[0,0,80],target_velocity=[0,0],target_status=0,target_fuse=0)


def log(states,scenario='incoming',reason='bounded_observation_complete'):
    meta=dict(scenario=scenario,target_level=9,supersonic=4400,bobomb_launched=1,
              horizontal_knockback=100,vertical_knockback=101,metal_flag=4,wing_flag=8,vanish_flag=2,
              default_bindings=1,ordinary_mode=0,online=states[0]['online'],
              network_role=states[0]['network_role'],muted=1,actor_writes=0)
    events=[('BEGIN',meta),('STAGE',dict(normal_entry=1)),('CAPTURE',dict(ms=2000,reason='entry'))]
    events += [('STATE',s) for s in states]
    events += [('END',dict(ms=3000,reason=reason))]
    return '\n'.join('ROCKET_COMBINED_QA_'+kind+' '+json.dumps(value) for kind,value in events)


class ObservedEvidenceTests(unittest.TestCase):
    def test_overlap_and_complete_route_are_not_native_damage(self):
        report=analyze(log([state(i) for i in range(3)]))
        self.assertTrue(report['run_valid'])
        self.assertFalse(report['observations']['native_target_damage'])
        self.assertFalse(report['route_evidence_complete'])
        self.assertFalse(report['full_feature_acceptance'])

    def test_actual_target_damage_required(self):
        samples=[state(i) for i in range(3)]
        samples[1].update(health=2112,hurt=4,interact_target=1,presentation=1,active=0)
        self.assertTrue(analyze(log(samples))['observations']['native_target_damage'])
        samples[1]['interact_target']=0
        self.assertFalse(analyze(log(samples))['observations']['native_target_damage'])

    def test_native_launch_requires_transition_and_subsonic_contact(self):
        samples=[state(i) for i in range(3)]
        samples[1].update(target_action=1,target_velocity=[25,30])
        self.assertTrue(analyze(log(samples,'bump'))['observations']['native_bobomb_launch'])
        samples[0]['speed']=4500
        self.assertFalse(analyze(log(samples,'bump'))['observations']['native_bobomb_launch'])

    def test_speed_without_native_knockback_is_not_attack(self):
        samples=[state(i) for i in range(3)]
        samples[0]['speed']=4500
        self.assertFalse(analyze(log(samples,'supersonic'))['route_evidence_complete'])
        samples[1]['target_action']=100
        samples[2]['target_action']=100
        self.assertTrue(analyze(log(samples,'supersonic'))['route_evidence_complete'])
        samples[1]['target_sync']=26
        self.assertFalse(analyze(log(samples,'supersonic'))['route_evidence_complete'])

    def test_confirmed_knockback_requires_safe_next_collision_pass(self):
        samples=[state(i) for i in range(3)]
        samples[0]['speed']=4500
        samples[1]['target_action']=101
        self.assertFalse(analyze(log(samples[:2],'supersonic'))['route_evidence_complete'])
        samples[2].update(target_action=101,health=2112,hurt=3,interact_target=1,active=0)
        report=analyze(log(samples,'supersonic'))
        self.assertTrue(report['observations']['supersonic_native_knockback'])
        self.assertTrue(report['observations']['supersonic_terminal_target_damage'])
        self.assertFalse(report['route_evidence_complete'])
        # A counter already draining before confirmation is not a new injury.
        samples[0].update(hurt=5,health=2176)
        samples[1].update(hurt=4,health=2112)
        samples[2].update(hurt=3,health=2048)
        self.assertTrue(analyze(log(samples,'supersonic'))['route_evidence_complete'])
        # Reused or different target identity cannot supply terminal followup.
        samples[2]['target_sync']=26
        self.assertFalse(analyze(log(samples,'supersonic'))['route_evidence_complete'])
        samples[2].update(target_sync=25,target_action=0)
        self.assertFalse(analyze(log(samples,'supersonic'))['route_evidence_complete'])
        samples[2].update(target_action=101,ms=samples[1]['ms']+201)
        self.assertFalse(analyze(log(samples,'supersonic'))['route_evidence_complete'])

    def test_focus_loss_and_nonfinite_are_rejected(self):
        samples=[state(i) for i in range(3)]
        self.assertFalse(analyze(log(samples,reason='focus_lost'))['run_valid'])
        samples[1]['fuel']=float('nan')
        self.assertFalse(analyze(log(samples))['run_valid'])

    def test_wing_allowance_needs_real_continuous_frames(self):
        samples=[state(i) for i in range(12)]
        for s in samples:s.update(caps=8,cap_timer=1000,topper=1,effective_mode=1,mapped=[.5,0,0,1,0])
        report=analyze(log(samples,'wing'))
        self.assertTrue(report['observations']['wing_preserves_finite_tank'])
        self.assertFalse(report['full_feature_acceptance'])
        bad=copy.deepcopy(samples)
        for s in bad:s['active']=0
        self.assertFalse(analyze(log(bad,'wing'))['observations']['wing_preserves_finite_tank'])
        samples[5]['fuel']=55
        self.assertFalse(analyze(log(samples,'wing'))['observations']['wing_preserves_finite_tank'])

    def test_offline_starter_is_not_an_online_lease(self):
        samples=[state(i) for i in range(12)]
        for s in samples:s.update(caps=8,cap_timer=1000,topper=1,effective_mode=1,mapped=[.5,0,0,1,0])
        report=analyze(log(samples,'wing'))
        self.assertTrue(report['route_evidence_complete'])
        self.assertTrue(report['observations']['offline_course_cap_observed'])
        self.assertFalse(report['observations']['online_course_lease_observed'])
        for s in samples:s.update(online=1,network_role='client')
        report=analyze(log(samples,'wing'))
        self.assertTrue(report['run_valid'])
        self.assertFalse(report['route_evidence_complete'])
        self.assertFalse(report['observations']['online_course_lease_observed'])
        for s in samples:s['cap_managed']=1
        report=analyze(log(samples,'wing'))
        self.assertTrue(report['route_evidence_complete'])
        self.assertTrue(report['observations']['online_course_lease_consistent'])
        self.assertEqual(report['network_roles_observed'],['client'])
        self.assertFalse(report['full_feature_acceptance'])
        samples[-1]['cap_managed']=0
        report=analyze(log(samples,'wing'))
        self.assertTrue(report['observations']['online_course_lease_observed'])
        self.assertFalse(report['route_evidence_complete'])

    def test_missing_lease_telemetry_or_invalid_role_is_rejected(self):
        samples=[state(i) for i in range(3)]
        del samples[1]['cap_managed']
        self.assertFalse(analyze(log(samples))['run_valid'])
        samples[1].update(cap_managed=1,online=1,network_role='none')
        self.assertFalse(analyze(log(samples))['run_valid'])


if __name__=='__main__':unittest.main()
