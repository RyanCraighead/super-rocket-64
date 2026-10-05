"""One setup worker. Writes only to its new staging directory."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(Path(__file__).parent))
import launcher as assets

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=('link','bomberman','banjo','spiderman','tony','octane'))
    for name in ('rom','game','ueviewer','output','scratch'):
        parser.add_argument('--'+name, type=Path)
    a=parser.parse_args()
    stage=a.output
    if a.character=='octane':
        scripts=ROOT/'codex/rocketleague/tools'
        export=a.scratch/'octane-export'
        for cmd in ([sys.executable,'-I','-B','-u',str(scripts/'export_octane.py'),'--game',str(a.game),'--ueviewer',str(a.ueviewer),'--out',str(export)],
                    [sys.executable,'-I','-B','-u',str(scripts/'convert_octane.py'),str(export),str(stage/'octane-model')]):
            result = subprocess.run(cmd, shell=False, capture_output=True, text=True, encoding='utf-8', errors='replace',
                                    creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
            if result.stdout: print(result.stdout, end='', flush=True)
            if result.stderr: print(result.stderr, end='', file=sys.stderr, flush=True)
            if result.returncode:
                raise ValueError('Octane conversion step failed: ' + Path(cmd[4]).name + '; ' + (result.stderr.strip() or result.stdout.strip()))
    elif a.character=='link':
        assets.validate_oot(a.rom)
        assets.extract(a.rom,stage/'oot-link')
        assets.convert(stage/'oot-link/manifest.json',stage/'oot-link/mesh-adult')
    elif a.character=='bomberman':
        assets.validate_bm64(a.rom)
        assets.convert_bm64(a.rom,stage/'bm64-bomberman')
    elif a.character=='banjo':
        assets.validate_bk(a.rom)
        assets.convert_bk(a.rom,stage/'bk-duo')
    elif a.character=='spiderman':
        assets.validate_spiderman(a.rom)
        assets.check_spiderman_prerequisite()
        assets.convert_spiderman(a.rom,stage/'spiderman-original')
    else:
        assets.check_spiderman_prerequisite()
        from codex.thps.mesh.decode_thps import export
        from codex.thps.mesh.original_poses import run
        export(a.rom,stage/'thps-original')
        run(a.rom,stage/'thps-original')

if __name__=='__main__':
    try:
        main()
    except (ValueError,OSError,ImportError,subprocess.SubprocessError) as error:
        print('Extraction stopped: '+str(error),file=sys.stderr)
        raise SystemExit(2)
