"""Export exact supported attack textures; no payload belongs in source control."""
import argparse,json,struct
from pathlib import Path
from codex.spiderman.assets.archive import load_rom,read_archive,table,sha256,ROM_SHA256
from codex.spiderman.mesh.decode_spiderman import texture
HASHES={38:'d1e94d04a0f1f8ac53548f22702e13b3986156c2da09601b875571b21bce7eef',111:'fa1efd4c0f11a4cad1dbe8c8fe6d904ba0dbef69605f266079f243526c7650ee',108:'91207872d6b22b53f7083a56a159a5c0994fd829d1955e17c63324141a6809a7'}
def records(rom):
    a=read_archive(load_rom(rom));boot=a['boot'];base=a['boot_load_base'];assert struct.unpack_from('>H',boot,0x800f51ec+9*2-base)[0]==38
    # Direct source constructor arguments, rather than filename inference.
    assert struct.unpack_from('>I',boot,0x800b2698-base)[0]&65535==111
    stream=a['groups'][3]['data'];entries=table(stream);result=[]
    for slot in(38,111,108):
        start,end=entries[slot];raw=stream[start:end];t=texture(raw);levels=t.pop('levels');assert len(levels)==1 and t['width']==t['height']==32;rgba=levels[0]['rgba'];assert len(rgba)==4096 and sha256(rgba)==HASHES[slot]
        result.append({'slot':slot,'texture':t,'file':f'web_attack_texture_{slot}_mip_0.rgba','rgba_sha256':sha256(rgba),'source_range':[start,end],'source_sha256':sha256(raw),'raw':raw,'rgba':rgba})
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--rom',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    if '.runtime'not in a.out.resolve().parts:raise SystemExit('Output must be in ignored .runtime storage')
    result=records(a.rom);a.out.mkdir(parents=True,exist_ok=True)
    for record in result:(a.out/record['file']).write_bytes(record['rgba']);del record['raw'];del record['rgba']
    manifest={'schema':'smn64.web.attack.textures.v1','rom_sha256':ROM_SHA256,'lookup':'projectileB2280 effect9/F51EC; burstB2698 direct111; decalB3D40/69EAC direct108','textures':result};(a.out/'web-attack-textures.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
