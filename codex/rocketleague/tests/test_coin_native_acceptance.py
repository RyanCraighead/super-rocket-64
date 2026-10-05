"""Synthetic negative evidence cases; these do not establish native acceptance."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from coin_native_acceptance import analyze


def fixture(driver='client'):
    own = 1 if driver == 'client' else 0
    key = dict(sync=0, parent=420, ordinal=1)
    event = dict(global_index=own, collector=own, authority=0, request=7, epoch=2,
                 area_sequence=5+own, authority_sequence=5, timer=200, value=1, active=1, **key)
    event['global'] = event.pop('global_index')
    rows = {role: [] for role in ('host', 'client')}
    for role, global_id in (('host', 0), ('client', 1)):
        rows[role].append(('PEER', dict(global_index=global_id, timer=100, parent=420, level=9, area=1,
             area_sequence=5+global_id, applications=0, fuel=40 if role == driver else 100, active=1, coins=0, actor_writes=0)))
        rows[role][-1][1]['global'] = rows[role][-1][1].pop('global_index')
        rows[role].append(('LOOT', dict(timer=180, value=1, tangible=1, consumed=0, **key, **{'global': global_id})))
        for timer, count, coins, live in ((150, 0, 0, 0), (190, 0, 0, 1), (270, int(role == driver), 1, 0)):
            rows[role].append(('ACCOUNT', dict(timer=timer, parent=420, applications=count, coins=coins, live=live, **{'global': global_id})))
    rows[driver] += [
        ('BEGIN', dict(ms=1000, timer=160, epoch=2, applications=0, fuel=40, coins=0, active=1, ordinary=0,
                       parent=420, actor_writes=0, **{'global': own})),
        ('EVENT', dict(event, stage='pickup')),
        ('EVENT', dict(event, stage='claim_send')),
        ('CREDIT', dict(event, serial=1, before=40, after=45, result=1, ordinary=0)),
        ('CAPTURE', dict(phase='before', ms=1500, timer=180, fuel=40, parent=420, ordinal=1)),
        ('CAPTURE', dict(phase='after', ms=5000, timer=280, fuel=45, parent=420, ordinal=1)),
        ('END', dict(reason='reward_collected', ms=5000, timer=280, epoch=2, applications=1,
                     fuel=45, coins=1, active=1, parent=420, ordinal=1)),
    ]
    other = 'client' if driver == 'host' else 'host'
    rows[other].append(('EVENT', dict(event, stage='claim_accept', **{'global': 1-own})))
    if driver == 'client':
        rows['host'].append(('EVENT', dict(event, stage='grant_send', **{'global': 0})))
        rows['client'].append(('EVENT', dict(event, stage='grant_received')))
    return rows


def logs(rows):
    return {role: '\n'.join('ROCKET_COIN_QA_'+kind+' '+json.dumps(row) for kind, row in data)
            for role, data in rows.items()}


def row(rows, role, kind):
    return next(value for name, value in rows[role] if name == kind)


class CoinNativeEvidenceTests(unittest.TestCase):
    def test_both_roles_and_real_capture_files(self):
        for driver in ('host', 'client'):
            data = logs(fixture(driver))
            self.assertTrue(analyze(data, driver)['passed'])
            with tempfile.TemporaryDirectory() as temporary:
                for ms in (1500, 5000):
                    (Path(temporary) / f'frame-{ms:05d}.ppm').write_bytes(b'P6\n1 1\n255\n\x11\x22\x33')
                result = analyze(data, driver, temporary)
                self.assertTrue(result['passed'])
                self.assertTrue(result['screenshot_files_verified'])
                self.assertFalse(result['full_feature_acceptance'])

    def test_no_credit_from_shared_score(self):
        data=fixture();data['client']=[r for r in data['client'] if r[0]!='CREDIT']
        self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_exact_five_required(self):
        for value in (40, 40.2, 44, 46, 50):
            data=fixture();row(data,'client','CREDIT')['after']=value
            self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_observer_at_100_requires_zero_applications(self):
        data=fixture();data['host'].append(('CREDIT',dict(row(data,'client','CREDIT'),before=100,after=100,**{'global':0})))
        self.assertFalse(analyze(logs(data),'client')['passed'])
        data=fixture()
        for kind,r in data['host']:
            if kind=='ACCOUNT':r['applications']=1
        self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_duplicate_grant_application_rejected(self):
        data=fixture();data['client'].append(('CREDIT',copy.deepcopy(row(data,'client','CREDIT'))))
        self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_native_identity_and_duplicate_reward(self):
        for key,value in (('parent',421),('ordinal',2),('sync',99)):
            data=fixture();row(data,'host','LOOT')[key]=value
            self.assertFalse(analyze(logs(data),'client')['passed'])
        data=fixture();data['host'].append(('LOOT',dict(row(data,'host','LOOT'),ordinal=2)))
        self.assertFalse(analyze(logs(data),'client')['passed'])
        data=fixture();row(data,'host','ACCOUNT')['live']=2
        self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_rival_claim_score_double_count_is_not_grant_success(self):
        data=fixture()
        [r for k,r in data['host'] if k=='ACCOUNT'][-1]['coins']=2
        self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_authority_epoch_and_source_must_match(self):
        for key,value in (('authority',1),('epoch',3),('collector',0),('area_sequence',99),('authority_sequence',99)):
            data=fixture();row(data,'client','CREDIT')[key]=value
            self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_live_or_intangible_reward_is_not_collected_reward(self):
        for key,value in (('tangible',0),):
            data=fixture();row(data,'host','LOOT')[key]=value
            self.assertFalse(analyze(logs(data),'client')['passed'])
        data=fixture();[r for k,r in data['host'] if k=='ACCOUNT'][-1]['live']=1
        self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_presentation_stale_balance_and_short_hold(self):
        for kind,key,value in (('CREDIT','active',0),('END','fuel',40),('END','timer',220),('END','epoch',3),('END','reason','timeout')):
            data=fixture();row(data,'client',kind)[key]=value
            self.assertFalse(analyze(logs(data),'client')['passed'])

    def test_missing_peer_capture_and_nonfinite(self):
        data=fixture();data['host']=[]
        self.assertFalse(analyze(logs(data),'client')['passed'])
        data=fixture();data['client']=[r for r in data['client'] if r[0]!='CAPTURE']
        self.assertFalse(analyze(logs(data),'client')['passed'])
        data=fixture();row(data,'client','CREDIT')['after']=float('nan')
        self.assertFalse(analyze(logs(data),'client')['passed'])
        with tempfile.TemporaryDirectory() as temporary:
            self.assertFalse(analyze(logs(fixture()),'client',temporary)['passed'])


if __name__=='__main__':
    unittest.main()
