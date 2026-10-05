"""Opt-in muted native arena entry and SDL-only boss driving; no state editing."""
import argparse, ctypes, hashlib, json, os
from pathlib import Path
import shutil, subprocess, time
from boss_acceptance import analyze
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser(description=__doc__)
for name in ('binary','rom','octane','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--sha256',required=True)
p.add_argument('--scenario',choices=('bowser','bob','bob-recovery'),required=True)
p.add_argument('--allow-focused-test',action='store_true')
p.add_argument('--orbit-delay-ms',type=int,default=900)
a=p.parse_args()
if not 0<=a.orbit_delay_ms<=2500:raise SystemExit('Orbit delay must be 0..2500 ms')
if os.name!='nt' or not a.allow_focused_test:raise SystemExit('Coordinate an idle controller and explicitly allow the isolated focused test')
def digest(path,algorithm='sha256'):
    with open(path,'rb') as f:return hashlib.file_digest(f,algorithm).hexdigest()
for name in ('binary','rom','octane','output'):setattr(a,name,getattr(a,name).resolve())
if digest(a.binary)!=a.sha256:raise SystemExit('Binary hash changed')
if digest(a.rom,'sha1')!='9bef1128717f958171a4afac3ed78ee2bb4e86ce':raise SystemExit('Supported private SM64 ROM required')
if not a.output.is_relative_to((ROOT/'codex/.runtime').resolve()):raise SystemExit('Use a new private codex/.runtime directory')
existing=subprocess.run(['powershell.exe','-NoProfile','-Command','Get-Process sm64coopdx* -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id'],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
if existing.stdout.strip():raise SystemExit('An SM64 game is open; preserve that session')
a.output.mkdir(parents=True,exist_ok=False)
save=a.output/'save';save.mkdir();shutil.copyfile(a.rom,save/'baserom.us.z64')
(save/'sm64config.txt').write_text('window_w 1280\nwindow_h 720\nfullscreen false\nmaster_volume 0\nmusic_volume 0\nsfx_volume 0\nenv_volume 0\nframe_limit 30\nframerate_mode 1\ninterpolation_mode 1\nbettercam_enable true\nbettercam_analog true\nbackground_gamepad 0\n')
user=ctypes.windll.user32;kernel=ctypes.windll.kernel32
user.GetForegroundWindow.restype=ctypes.c_void_p
user.GetWindowThreadProcessId.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
user.SetForegroundWindow.argtypes=[ctypes.c_void_p]
user.ShowWindow.argtypes=[ctypes.c_void_p,ctypes.c_int]
user.BringWindowToTop.argtypes=[ctypes.c_void_p]
callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
def focus(pid):
    found=[]
    @callback
    def visit(window,_):
        owner=ctypes.c_ulong();user.GetWindowThreadProcessId(window,ctypes.byref(owner))
        if owner.value==pid and user.IsWindowVisible(ctypes.c_void_p(window)):found.append(window);return False
        return True
    user.EnumWindows(visit,None)
    if not found:return False
    window=found[0];owner=ctypes.c_ulong()
    foreground=user.GetWindowThreadProcessId(user.GetForegroundWindow(),ctypes.byref(owner));own=kernel.GetCurrentThreadId()
    joined=foreground!=own and user.AttachThreadInput(own,foreground,True)
    try:
        user.ShowWindow(window,9);user.BringWindowToTop(window);return bool(user.SetForegroundWindow(window))
    finally:
        if joined:user.AttachThreadInput(own,foreground,False)
env=dict(os.environ,SDL_AUDIODRIVER='dummy',SM64_ROCKET_QA_OUTPUT=str(a.output),SM64_ROCKET_QA_GAMEPAD='1',SM64_ROCKET_QA_BOSS=a.scenario,SM64_ROCKET_QA_BOSS_ORBIT_DELAY=str(a.orbit_delay_ms))
for key in ('THPS_SCORE_SCENARIO','SM64_QA_OUTPUT_DIR','SM64_ROCKET_QA_HOST','SM64_ROCKET_QA_DOOR','SM64_ROCKET_QA_INTERRUPTS','SM64_CHARACTER_NET_OBSERVE','SM64_CHARACTER_NET_MOTION','SM64_CHARACTER_NET_SECONDS'):env.pop(key,None)
command=[str(a.binary),'--console','--offline','--disable-mods','--hide-loading-screen','--skip-intro','--skip-update-check','--no-discord','--windowed','--savepath',str(save),'--rocket-car',str(a.octane),'--backend','opengl']
with (a.output/'stdout.log').open('w') as out,(a.output/'stderr.log').open('w') as err:
    process=subprocess.Popen(command,cwd=ROOT,env=env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        focused=False
        for _ in range(100):
            if process.poll() is not None:break
            focused=focus(process.pid)
            if focused:break
            time.sleep(.1)
        print('Native boss QA started',process.pid,'focus',focused,flush=True)
        timed_out=False
        try:code=process.wait(timeout=220)
        except subprocess.TimeoutExpired:
            timed_out=True;process.terminate();code=process.wait(timeout=5)
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
log=(a.output/'stderr.log').read_text(errors='replace')
evidence=analyze(log,a.scenario)
acceptance=evidence['acceptance']
passed=not timed_out and code==0 and focused and evidence['passed']
record=dict(scenario=a.scenario,binary_sha256=a.sha256,exit_code=code,focused=focused,orbit_delay_ms=a.orbit_delay_ms,
    native_warp_staging='normal arena entry' if a.scenario=='bowser' else 'native cave link 0x0B to upper-mountain destination 0x0C',
    timed_out=timed_out,input='virtual SDL controller in own window only',
    direct_actor_state_writes=False,impact_observed='ROCKET_BOSS_IMPACT' in log,
    native_throw_and_damage_observed=bool(acceptance.get('thrown') and acceptance.get('damage')),
    native_complete=passed,acceptance=acceptance,evidence_checks=evidence['checks'],muted=True)
(a.output/'result.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
raise SystemExit(0 if passed else 1)
