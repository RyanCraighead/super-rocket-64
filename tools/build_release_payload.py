"""Build the standalone Windows release payload from this public source tree."""
import argparse,hashlib,json,sys,zipfile
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--engine',required=True,type=Path)
parser.add_argument('--dependencies',required=True,type=Path,help='Directory containing the three pinned official Python/Unicorn archives')
parser.add_argument('--output',required=True,type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
output=args.output.resolve()
downloads=args.dependencies.resolve()
output.mkdir(exist_ok=True)
files={}
def sha(data):return hashlib.sha256(data).hexdigest()
def add(path,name):
    assert not name.startswith('.') and '..' not in Path(name).parts and name not in files,name
    files[name]=Path(path).read_bytes()
# Exact runtime source allowlist; no private packaging module is required.
RUNTIME_SOURCES = ('codex/windows/THIRD_PARTY_NOTICES.txt', 'codex/windows/launcher.py', 'codex/windows/prepare_spiderman.py', 'codex/windows/spiderman-runtime-fingerprints.json', 'codex/spiderman/inspect_n64_input.py', 'codex/spiderman/assets/archive.py', 'codex/spiderman/assets/UPSTREAM-MIT.txt', 'codex/spiderman/mesh/decode_spiderman.py', 'codex/spiderman/mesh/export_original_poses.py', 'codex/spiderman/mesh/pose.py', 'codex/spiderman/movement/original_mips.py', 'codex/spiderman/markers/decode_markers.py', 'codex/spiderman/combat/extract_combat.py', 'codex/spiderman/web/export_web_textures.py', 'codex/spiderman/trail_render/export_trail_texture.py', 'codex/spiderman/web_attack_render/export_textures.py', 'codex/spiderman/dome_render/export_assets.py', 'codex/oot/assets/extract_oot_assets.py', 'codex/oot/assets/oot_ntsc_10_metadata.json', 'codex/oot/assets/oot_ntsc_12_metadata.json', 'codex/oot/mesh/decode_link_mesh.py', 'codex/oot/mesh/pose.py', 'codex/oot/mesh/__init__.py', 'codex/bm64/assets/prepare_bm64.py', 'codex/bm64/assets/bm64_us_10_metadata.json', 'codex/bm64/mesh/decode_bm64_mesh.py', 'codex/bm64/mesh/animation.py', 'codex/bm64/mesh/pose.py', 'codex/bm64/mesh/__init__.py', 'codex/bk/assets/prepare_bk.py', 'codex/bk/mesh/decode_bk_mesh.py', 'codex/bk/mesh/animation.py', 'codex/bk/UPSTREAM-CC0.txt', 'codex/bk/movement/LICENSE.upstream', 'codex/assets/DejaVu-LICENSE.txt')
for name in RUNTIME_SOURCES:
    add(root/name,name)
for name in ('codex/windows/seven_launcher.py','codex/windows/download_ueviewer.py','codex/windows/extract_character.py',
             'codex/thps/assets/archive.py','codex/thps/mesh/decode_thps.py','codex/thps/mesh/original_poses.py',
             'codex/thps/mechanics/air_spin_export_rotations.py','codex/thps/assets/UPSTREAM-MIT.txt',
             'codex/rocketleague/tools/local_aes.py','codex/rocketleague/tools/export_octane.py','codex/rocketleague/tools/convert_octane.py'):
    add(root/name,name)
for name in ('engine_setup.py','owned_audio_setup.py','engine_recipe.json','owned-audio-recipe.json','owned_ctl_setup.py','custom_visual_setup.py','source_audio_setup.py'):
    add(root/'codex/windows'/name,'codex/windows/'+name)
recipe=json.loads((root/'codex/windows/engine_recipe.json').read_text())
assert all(entry['method'] not in ('seed','audio') for entry in recipe['files'].values()), 'A private full-buffer seed dependency remains'
for group in ('ctl_tools','ctl_inputs','visual_inputs','audio_inputs'):
    for p in sorted((root/'codex/windows'/group).rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts:
            add(p,p.relative_to(root).as_posix())
assert not any('private_engine_seed' in name for name in files)
for folder in ('lang','palettes'):
    for p in sorted((root/folder).glob('*.ini')):add(p,p.relative_to(root).as_posix())
game=args.engine.resolve()
assert game.is_file(),'Wait for candidate build'
add(game,'sm64coopdx.exe')
pins={
    'python-3.13.16-embeddable-amd64.zip':'589dc1e9d02549ca5f680710307ff9b77f6e3ffc4aa3efccd7fa54eeeafac94b',
    'unicorn-2.1.4-cp37-abi3-win_amd64.whl':'d7107500c64ce5c168fbff6bef9485b5db1350050036f4cea568650cf8bdbdf5',
    'unicorn-2.1.4.tar.gz':'00567a70e323f749b419cd86bee4f9115beab7ebba32194581c090cbb7c59cff'}
for name,digest in pins.items():assert sha((downloads/name).read_bytes())==digest,name
for name,prefix in [('python-3.13.16-embeddable-amd64.zip','python/'),('unicorn-2.1.4-cp37-abi3-win_amd64.whl','python/Lib/site-packages/')]:
    with zipfile.ZipFile(downloads/name) as archive:
        assert archive.testzip() is None
        for item in archive.infolist():
            if item.is_dir():continue
            assert not Path(item.filename).is_absolute() and '..' not in Path(item.filename).parts
            files[prefix+item.filename]=archive.read(item)
files['python/python313._pth']=b'python313.zip\n.\nLib/site-packages\n..\n../codex\n../codex/windows\n../codex/rocketleague/tools\n'
add(downloads/'unicorn-2.1.4.tar.gz','licenses/unicorn-2.1.4.tar.gz')
add(root/'codex/windows/single_exe/EMBEDDED-RUNTIME.txt','licenses/EMBEDDED-RUNTIME.txt')
add(root/'codex/rocketleague/vendor/RocketSim/LICENSE','licenses/RocketSim-MIT.txt')
for notice in sorted((root/'licenses/baseline').iterdir()):
    if notice.is_file(): add(notice,'licenses/baseline/'+notice.name)
# Source-export notice provenance is already relative or an official URL.
for name in ('RETAINED-RESOURCES.json','RETAINED-RESOURCE-SCOPE.md'):
    add(root/'licenses'/name,'licenses/'+name)
files['INSTALL-AND-PLAY.txt']=b'''Super Rocket 64 - Windows x64
Run this launcher again to play; it preserves versioned program files and data.
Setup: select your supported original US SM64 ROM and audited Rocket League
Windows installation (Epic Games Store or Steam; supported package versions). The pinned extraction tool is provisioned as needed.
Choose Yes or No for optional games; only selected characters need extra ROMs.
Offline starts the game as Octane. Hold F7 or Share/Back to open the wheel.
Online supports Mario and Octane on matching builds: choose LAN/Tailscale,
then Host or Join.
Host defaults to port 7777; share a reachable LAN/Tailscale address and port.
Join uses your host's IP/address and port. Configure Tailscale yourself.
No firewall, VPN or security settings are changed. Keep source games and saves
private. Cancel/retry and cached verified assets are supported.
This baseline does not create shortcuts or check for new releases.
See licenses/baseline and codex/windows/THIRD_PARTY_NOTICES.txt for attribution.
'''
manifest={'schema_version':4,'edition':'super-rocket-64','release_status':'public-preview','upstream_commit':'8cd6e5977d9f920d51ca71f2c61801d019ed79c6','files':{name:{'size':len(data),'sha256':sha(data)} for name,data in sorted(files.items())}}
files['PACKAGE-MANIFEST.json']=(json.dumps(manifest,indent=2)+'\n').encode()
payload=output/'Payload.zip'
assert not payload.exists(),'Choose a new output directory'
with zipfile.ZipFile(payload,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
    for name,data in sorted(files.items()):archive.writestr(name,data)
digest=sha(payload.read_bytes())
namespace='SuperRocket64' if 'namespace SuperRocket64' in (root/'codex/windows/single_exe/Bootstrap.cs').read_text() else 'N64CodexLab'
(output/'PayloadInfo.cs').write_text(f'namespace {namespace} {{ internal static class PayloadInfo {{ internal const string ZipSha256="{digest}"; internal const long ZipSize={payload.stat().st_size}L; }} }}')
(output/'build-record.json').write_text(json.dumps({'release_candidate':True,'payload_sha256':digest,'files':len(files),'private_engine_seed_files':0,'game_sha256':sha(game.read_bytes()),'dependency_archive_sha256':pins},indent=2))
print('Release payload built:',len(files),'files; zero private build seeds.')

