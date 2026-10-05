"""Real Windows config/preset acceptance without assets, window or network."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument('--engine',required=True,type=Path)
p.add_argument('--report',type=Path)
a=p.parse_args()
checks=0
def check(value,message):
    global checks
    checks+=1
    assert value,message
def probe(save,preset=None,expected=0):
    env=dict(os.environ);env.pop('SUPER_ROCKET64_CONTROLS',None)
    r=subprocess.run([str(a.engine),'--verify-difficulty',str(save)]+([] if preset is None else [str(preset)]),
                     capture_output=True,env=env,timeout=30,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    check(r.returncode==expected,(r.returncode,r.stdout,r.stderr))
    if expected:return None
    line=next(x for x in r.stdout.decode().splitlines() if x.startswith('DIFFICULTY '))
    return tuple(map(int,line.split()[1:]))
with tempfile.TemporaryDirectory(prefix='sr64-difficulty-') as tmp:
    root=Path(tmp)
    for layout in ('persistent data','portable folder'):
        save=root/layout/'data/.runtime/combined/save';save.mkdir(parents=True)
        config=save/'sm64config.txt';marker=save/'progress.sav';marker.write_bytes(b'fixture-progress\0\xff')
        check(probe(save)==(75,50,1),'Fresh install must be Medium')
        config.write_text('rocket_speed_percent 88\nrocket_jump_height_percent 67\nkey_a 002c 1003 1103\nrocket_boost_mode 1\nrocket_surface_mode 0\n')
        original=config.read_bytes()
        check(probe(save)==(88,67,3),'Upgrade must retain custom pair')
        check(config.read_bytes()==original,'Loading custom values rewrote source')
        for preset,pair in enumerate(((100,100),(75,50),(50,30))):
            check(probe(save,preset)==(*pair,preset),'Preset apply failed')
            check(probe(save)==(*pair,preset),'Preset restart failed')
            content=config.read_text()
            check('key_a 002c 1003 1103' in content,'Preset changed native bindings')
            check('rocket_boost_mode 1' in content and 'rocket_surface_mode 0' in content,'Preset changed unrelated gameplay')
            check(not (save/'sm64config.txt.tmp').exists(),'Staged file leaked')
        for content,expected in (
            ('rocket_speed_percent 74\nrocket_jump_height_percent 51\n',(74,51,3)),
            ('rocket_speed_percent 88\n',(88,50,3)),
            ('rocket_jump_height_percent 30\n',(75,30,3)),
            ('rocket_speed_percent 49\nrocket_jump_height_percent 29\n',(75,50,1)),
            ('rocket_speed_percent broken\nrocket_jump_height_percent 101\n',(75,50,1)),
            ('rocket_difficulty 0\nrocket_speed_percent 88\nrocket_jump_height_percent 67\n',(88,67,3)),
            ('show_fps true\n',(75,50,1))):
            config.write_text(content);check(probe(save)==expected,'Missing/invalid/manual/stale preset value')
        config.write_text('rocket_speed_percent 88\nrocket_jump_height_percent 67\n')
        before=config.read_bytes()
        for bad in ('3','99','-1','broken'):probe(save,bad,2);check(config.read_bytes()==before,'Invalid preset changed saved pair')
        stage=save/'sm64config.txt.tmp';stage.mkdir()
        probe(save,2,2);check(config.read_bytes()==before,'Failed preset write changed saved pair')
        check(probe(save)==(88,67,3),'Failed write lost custom settings')
        stage.rmdir();check(probe(save,2)==(50,30,2),'Retry after save failure failed')
        check(marker.read_bytes()==b'fixture-progress\0\xff','Preset altered progression')
report={'passed':True,'checks':checks,'actual_engine_config_and_preset_functions':True,'layouts':2,'game_started':False,'foreground_used':False}
if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
