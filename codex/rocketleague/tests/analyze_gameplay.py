"""Validate observed native QA logs. Does not synthesize gameplay evidence."""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path

def analyze(path,gamepad=False):
    text=path.read_text()
    if 'ROCKET_QA_BEGIN' not in text or 'ROCKET_QA_END' not in text or 'Rocket draw failed' in text:
        raise ValueError('Incomplete or failed native run')
    states=[json.loads(line.split(' ',1)[1]) for line in text.splitlines() if line.startswith('ROCKET_QA_STATE ')]
    assert len(states)>=150 and all(s['active'] and s['focus'] for s in states)
    assert all(math.isfinite(v) for s in states for key in ('pos','vel','up') for v in s[key])
    assert any(s['jump'] for s in states) and any(s['double'] for s in states) and any(s['flip'] for s in states)
    assert min(s['up'][1] for s in states)<-.5
    assert max(s['pos'][1] for s in states)-states[0]['pos'][1]>500
    assert any(not s['ground'] for s in states) and states[-1]['ground']
    paused=[s for s in states if 6200<=s['ms']<=6650]
    assert len(paused)>=5 and all(s['paused'] for s in paused)
    assert all(s['ticks']==paused[0]['ticks'] and s['pos']==paused[0]['pos'] for s in paused)
    held=[s for s in states if 7100<=s['ms']<=7450]
    assert len(held)>=4 and all(not s['paused'] and not s['jump'] and s['boost']==100 for s in held)
    focused=[s for s in states if 9350<=s['ms']<=9650]
    assert len(focused)>=4 and all(s['panel'] and s['stick']==[0,0] and not s['buttons'] for s in focused)
    assert all(s['boost']==focused[0]['boost'] for s in focused) and focused[-1]['ticks']>focused[0]['ticks']
    if not gamepad:assert focused[-1]['pos'][1]<focused[0]['pos'][1]
    assert 70<min(s['boost'] for s in states)<(95 if gamepad else 90)
    assert max(math.sqrt(sum(v*v for v in s['vel'])) for s in states)<=4601
    assert all(s['ticks']%4==0 for s in states)
    assert all(b['ticks']>=a['ticks'] and b['ticks']-a['ticks']<=4*(b['timer']-a['timer']) for a,b in zip(states,states[1:]))
    captures=[line for line in text.splitlines() if line.startswith('ROCKET_QA_CAPTURE ')]
    assert len(captures)==5 and all('renderer=' in c for c in captures)
    result=dict(schema='rocket-native-observation-v1',log_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                observations=len(states),duration_ms=states[-1]['ms'],physics_ticks=states[-1]['ticks'],
                original_rl_parity=False,checks=['native original mesh load/draw','jump/double jump','directional flip and landing',
                'boost and steering','pause freezes ticks/position','held buttons suppressed on resume',
                'panel blocks controls while gravity continues','no catch-up stepping','five actual framebuffer captures'],
                min_boost=min(s['boost'] for s in states),minimum_up_y=min(s['up'][1] for s in states),
                maximum_speed_host_units_per_second=max(math.sqrt(sum(v*v for v in s['vel'])) for s in states),
                renderer=captures[0].split('renderer=',1)[1])
    return result

def analyze_host(path):
    text=path.read_text()
    states=[json.loads(line.split(' ',1)[1]) for line in text.splitlines() if line.startswith('HOST_QA_STATE ')]
    assert len(states)>=80
    name=states[0]['character']
    assert f'HOST_QA_BEGIN {name} master_volume=0' in text and f'HOST_QA_END {name}' in text
    assert all(s['character']==name and s['health']>0 for s in states)
    assert all(math.isfinite(v) for s in states for key in ('pos','vel') for v in s[key])
    assert math.dist(states[0]['pos'],states[-1]['pos'])>100
    paused=[s for s in states if 3200<=s['ms']<=3900]
    focused=[s for s in states if 4900<=s['ms']<=5300]
    assert len(paused)>=8 and all(s['paused'] and s['pos']==paused[0]['pos'] for s in paused)
    assert len(focused)>=4 and all(s['panel'] and s['pos']==focused[0]['pos'] for s in focused)
    assert not states[-1]['paused'] and not states[-1]['panel']
    captures=[line for line in text.splitlines() if line.startswith('ROCKET_QA_CAPTURE ')]
    assert len(captures)==5 and all('renderer=' in c for c in captures)
    return dict(schema='rocket-host-regression-observation-v1',character=name,
                log_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),observations=len(states),
                checks=['native load and movement','pause freezes position','panel blocks movement','resume and focus release','five actual framebuffer captures'],
                observed_height_range=max(s['pos'][1] for s in states)-min(s['pos'][1] for s in states),
                full_character_parity=False)

def analyze_gamepad(path):
    result=analyze(path,gamepad=True)
    result['checks']=[c.replace('panel blocks controls while gravity continues','panel blocks controls while simulation continues on the lawn') for c in result['checks']]
    text=path.read_text()
    assert 'ROCKET_QA_GAMEPAD standardized SDL virtual controller' in text
    mapped=[json.loads(line.split(' ',1)[1]) for line in text.splitlines() if line.startswith('ROCKET_QA_MAPPED ')]
    acceleration=[s for s in mapped if 8100<=s['ms']<=8350]
    pitch=[s for s in mapped if 4170<=s['ms']<=4370]
    braking=[s for s in mapped if 11300<=s['ms']<=11450]
    roll=[s for s in mapped if 10500<=s['ms']<=11000]
    assert len(acceleration)>=3 and all(s['throttle']==1 and s['pitch']==0 and s['boost'] for s in acceleration)
    assert len(pitch)>=2 and all(s['pitch']==-1 and s['throttle']==0 for s in pitch)
    assert len(braking)>=2 and all(s['throttle']==-1 and not s['pitch'] and not s['slide'] for s in braking)
    assert len(roll)>=5 and all(s['roll']==-1 and s['yaw']==0 and s['slide'] for s in roll)
    result['checks']+=['SDL gamepad trigger/pitch separation','analog brake input','standard face buttons','Square air roll/slide mapping']
    result['physical_playstation_driver_verified']=False
    return result

def analyze_interrupts(path):
    text=path.read_text()
    for marker in ('ROCKET_INPUT_QA_DETACH','ROCKET_INPUT_QA_RECONNECT_HELD','ROCKET_INPUT_QA_END'):assert marker in text
    states=[json.loads(line.split(' ',1)[1]) for line in text.splitlines() if line.startswith('ROCKET_INPUT_QA_STATE ')]
    assert len(states)>=100 and all(s['focus'] for s in states)
    opened=[s for s in states if 1000<=s['ms']<=1900]
    resumed=[s for s in states if 2150<=s['ms']<=2900]
    reconnected=[s for s in states if 4700<=s['ms']<=5900]
    assert len(opened)>=10 and all(s['console'] and s['raw_jump'] and s['raw_boost'] and not s['requested_jump'] and not s['requested_boost'] and not s['jump'] and s['boost']==100 for s in opened)
    for selected in (resumed,reconnected):
        # The combined binding layer suppresses held input before it reaches
        # rocket_runtime_last_input. Prove the real device buttons stay held
        # while the filtered requests and physical effects remain neutral.
        assert len(selected)>=8 and all(not s['console'] and s['raw_jump'] and s['raw_boost'] and not s['requested_jump'] and not s['requested_boost'] and not s['jump'] and s['boost']==100 for s in selected)
    assert any(s['raw_jump'] and s['requested_jump'] and s['jump'] for s in states if 6350<=s['ms']<=6450)
    assert any(s['raw_boost'] and s['requested_boost'] and s['boost']<100 for s in states if 6550<=s['ms']<=6700)
    assert 90<min(s['boost'] for s in states)<98
    return dict(schema='rocket-gamepad-interruption-observation-v2',observations=len(states),
                log_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                checks=['actual DJUI console captures gamepad','held jump/boost suppressed after menu close',
                        'SDL virtual unplug/reconnect with held buttons','release and re-press restores jump/boost'],
                physical_playstation_driver_verified=False)

def analyze_door(path):
    text=path.read_text()
    assert 'ROCKET_DOOR_QA_END transition=1 entered=1 car_resumed=1' in text
    assert 'ROCKET_DOOR_QA_TIMEOUT' not in text
    states=[json.loads(line.split(' ',1)[1]) for line in text.splitlines() if line.startswith('ROCKET_DOOR_QA_STATE ')]
    assert len(states)>=20 and all(s['focus'] for s in states)
    outside=[s for s in states if s['level']==16 and s['active']]
    inside=[s for s in states if s['level']==6 and s['active']]
    assert outside and inside and inside[0]['ms']>outside[0]['ms']
    assert any(s['action'] in (0x1320,0x1321) and not s['active'] for s in states)
    assert len(re.findall(r'^ROCKET_QA_CAPTURE ',text,re.M))>=2
    return dict(schema='rocket-native-door-observation-v1',observations=len(states),
                log_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                checks=['virtual gamepad drive from castle grounds','native door cutscene relinquishes car',
                        'native warp into castle','car resumes inside','actual framebuffer captures'],
                original_rl_parity=False)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('log',type=Path);parser.add_argument('--out',type=Path)
    group=parser.add_mutually_exclusive_group();group.add_argument('--host',action='store_true');group.add_argument('--gamepad',action='store_true');group.add_argument('--interrupts',action='store_true');group.add_argument('--door',action='store_true')
    args=parser.parse_args();run=analyze_host if args.host else analyze_gamepad if args.gamepad else analyze_interrupts if args.interrupts else analyze_door if args.door else analyze
    result=run(args.log);encoded=json.dumps(result,indent=2)+'\n'
    if args.out:args.out.write_text(encoded)
    print(encoded)
