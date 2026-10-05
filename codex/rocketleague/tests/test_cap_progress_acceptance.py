"""Synthetic reader regressions only; never native gameplay evidence."""
import copy
import json
import unittest
from cap_progress_acceptance import analyze


def sample(frame):
    return dict(ms=2000+(frame-100)*34,timer=frame,level=29,phase=2,active=1,ground=1,wheels=4,
                platform=-1,action=0,health=2176,dialog=-1,save=0,caps=8,visual_caps=8,
                cap_timer=1200-(frame-100),managed=0,online=0,role='none',epoch=2,ticks=frame*4,
                pos=[0,-2013,10],vel=[0,0,0],fuel=50,topper=1,switch=[10,101,1,0],
                box=[20,102,1,-1],cap_id=[-1,0],cap_valid=0,cap_active=0,cap_age=-1,
                cap_intangible=-1,cap_status=0,cap_parent=-1,cap_pos=[0,0,0],interact_cap=0,
                sent=[0,0,0,0,0,0])


def route():
    rows=[sample(frame) for frame in (100,101,102,103,104,105,106,126,127,1027,1926,1927,1960)]
    rows[1].update(platform=10,pos=[0,-1766,10])
    for s in rows[2:]:s.update(save=2,switch=[10,101,3,30],box=[20,102,2,0])
    rows[2].update(switch=[10,101,2,0],platform=10,dialog=10,active=0)
    rows[5]['box']=[20,102,3,-1]
    for s in rows[6:]:s.update(box=[20,102,5,-1],cap_id=[30,103],cap_parent=20,cap_pos=[0,-1960,-600],pos=[0,-2013,-600])
    rows[6].update(cap_valid=1,cap_active=1,cap_age=1,cap_intangible=-1)
    rows[7].update(cap_valid=1,cap_active=1,cap_age=21,cap_intangible=0)
    for s in rows[8:11]:s.update(cap_timer=1800-(s['timer']-127),interact_cap=1)
    for s in rows[11:]:s.update(caps=0,visual_caps=0,cap_timer=0,topper=0)
    rows[-1]['pos']=[30,-2013,-600]
    rows[-1]['sent'][0]=.4
    return rows


def log(rows=None,reason='bounded_observation_complete',complete=1,initial_save=0):
    rows=route() if rows is None else rows
    meta=dict(scenario='wing',target_level=29,save_flag=2,wing_flag=8,entry_duration=1200,
              pickup_duration=1800,initial_save=initial_save,online=0,network_role='none',
              muted=1,actor_writes=0,max_ms=128000)
    events=[('BEGIN',meta),('STAGE',dict(ms=500,normal_entry=1,level=29))]
    events += [('STATE',s) for s in rows]
    events += [('CAPTURE',dict(ms=rows[i]['ms'],reason=r)) for i,r in
               ((2,'switch_unlocked'),(8,'native_cap_collected'),(-1,'post_pickup_expired'))]
    events.append(('END',dict(ms=rows[-1]['ms']+1,reason=reason,complete=complete)))
    return '\n'.join('ROCKET_CAP_PROGRESS_'+kind+' '+json.dumps(value) for kind,value in events)


class CapProgressEvidenceTests(unittest.TestCase):
    def test_complete_native_sequence_is_only_narrow_route_acceptance(self):
        result=analyze(log())
        self.assertTrue(result['passed'],result)
        self.assertFalse(result['full_feature_acceptance'])
        self.assertEqual(result['event_frames'],dict(unlock=102,pickup=127,expiry=1927))

    def test_preexisting_unlock_and_native_mario_press_do_not_prove_car_switch(self):
        self.assertFalse(analyze(log(initial_save=2))['checks']['admitted_fresh_save'])
        rows=route()
        for s in rows:s['platform']=-1
        self.assertFalse(analyze(log(rows))['checks']['switch_pressed_by_car'])
        rows=route();rows[1]['active']=0
        self.assertFalse(analyze(log(rows))['checks']['switch_pressed_by_car'])

    def test_box_break_without_native_child_or_collection_is_insufficient(self):
        rows=route()
        for s in rows:s['cap_parent']=-1
        result=analyze(log(rows))
        self.assertTrue(result['checks']['unlocked_box_broken'])
        self.assertFalse(result['checks']['native_child_collected'])
        rows=route()
        for s in rows[8:11]:s['cap_timer']=max(1,1200-(s['timer']-100))
        self.assertFalse(analyze(log(rows))['checks']['native_child_collected'])

    def test_far_despawn_or_missing_native_recipient_is_not_pickup(self):
        rows=route();rows[7]['cap_pos']=[0,1000,-600]
        self.assertFalse(analyze(log(rows))['checks']['native_child_collected'])
        rows=route()
        for s in rows:s['interact_cap']=0
        self.assertFalse(analyze(log(rows))['checks']['native_child_collected'])

    def test_online_requires_verified_lease(self):
        rows=route()
        for s in rows:s.update(online=1,role='client',interact_cap=0)
        self.assertFalse(analyze(log(rows))['passed'])
        for s in rows[8:11]:s['managed']=1
        self.assertTrue(analyze(log(rows))['passed'])
        rows[9]['managed']=0
        self.assertFalse(analyze(log(rows))['checks']['managed_online_pickup'])

    def test_online_grant_can_precede_source_actor_deactivation(self):
        rows=route()
        for s in rows:s.update(online=1,role='client',interact_cap=0)
        rows[7].update(cap_timer=1800,managed=1)
        for s in rows[8:11]:s.update(cap_timer=1800-(s['timer']-126),managed=1)
        rows[10].update(caps=0,visual_caps=0,topper=0)
        self.assertTrue(analyze(log(rows))['passed'])

    def test_online_child_collection_on_first_observed_spawn_frame(self):
        # Native QA8: the box child spawned, touched the car and renewed the
        # verified lease in one native frame, then unlinked on the next frame.
        rows=route()
        for s in rows:s.update(online=1,role='server',managed=1)
        rows[6].update(cap_timer=1800,cap_intangible=0,interact_cap=1)
        for i in (7,8):rows[i].update(timer=100+i,ms=2000+i*34,ticks=(100+i)*4)
        rows[7].update(cap_active=0,cap_valid=0)
        for s in rows[7:10]:s.update(cap_timer=1800-(s['timer']-106),interact_cap=1)
        for s in rows[10:]:s.update(caps=0,visual_caps=0,cap_timer=0,topper=0)
        result=analyze(log(rows))
        self.assertTrue(result['passed'],result)
        self.assertEqual(result['event_frames']['pickup'],107)
        far=copy.deepcopy(rows);far[6]['cap_pos']=[0,1000,-600]
        self.assertFalse(analyze(log(far))['checks']['native_child_collected'])
        unleased=copy.deepcopy(rows);unleased[6]['managed']=0
        self.assertFalse(analyze(log(unleased))['checks']['native_child_collected'])

    def test_forced_timer_clear_is_not_natural_expiry(self):
        rows=route()
        for s in rows[9:11]:s.update(caps=0,visual_caps=0,cap_timer=0,topper=0)
        self.assertFalse(analyze(log(rows))['checks']['natural_post_pickup_expiry'])
        rows=route();rows[9]['cap_timer']=2
        self.assertFalse(analyze(log(rows))['checks']['natural_post_pickup_expiry'])

    def test_stale_visuals_and_no_resumed_motion_fail(self):
        rows=route();rows[11]['topper']=1
        self.assertFalse(analyze(log(rows))['checks']['post_expiry_visual_clear'])
        rows=route();rows[-1]['pos']=rows[-2]['pos'][:]
        self.assertFalse(analyze(log(rows))['checks']['control_resumed'])
        rows=route();rows[-1]['sent'][0]=0
        self.assertFalse(analyze(log(rows))['checks']['control_resumed'])

    def test_capture_labels_without_matching_event_time_do_not_pass(self):
        text=log().replace('"ms": '+str(route()[-1]['ms'])+', "reason": "post_pickup_expired"',
                           '"ms": 3000, "reason": "post_pickup_expired"')
        self.assertFalse(analyze(text)['checks']['meaningful_capture_records'])

    def test_focus_or_explicit_route_failure_never_passes(self):
        for reason in ('focus_lost','switch_jump_missed','bounded_timeout','stop_requested'):
            self.assertFalse(analyze(log(reason=reason,complete=0))['passed'])

    def test_identity_reuse_duplicate_keys_and_nonfinite_are_rejected(self):
        rows=route();rows[8]['cap_id']=[30,104]
        self.assertFalse(analyze(log(rows))['checks']['well_formed_evidence'])
        self.assertFalse(analyze(log().replace('"actor_writes": 0','"actor_writes": 0, "actor_writes": 0'))['checks']['well_formed_evidence'])
        rows=route();rows[5]['fuel']=float('nan')
        self.assertFalse(analyze(log(rows))['checks']['well_formed_evidence'])


if __name__=='__main__':unittest.main()
