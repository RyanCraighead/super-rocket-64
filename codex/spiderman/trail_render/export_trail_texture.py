"""Export the verified original connected-ribbon texture into private storage."""
import argparse,json,struct
from pathlib import Path
from codex.spiderman.assets.archive import load_rom,read_archive,table,sha256,ROM_SHA256
from codex.spiderman.mesh.decode_spiderman import texture

def main():
    p=argparse.ArgumentParser();p.add_argument('--rom',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    if '.runtime'not in a.out.resolve().parts:raise SystemExit('Output must remain in ignored .runtime storage')
    arc=read_archive(load_rom(a.rom));slot=struct.unpack_from('>H',arc['boot'],0x800f51ec+2*2-arc['boot_load_base'])[0];assert slot==41
    stream=arc['groups'][3]['data'];start,end=table(stream)[slot];raw=stream[start:end];t=texture(raw);levels=t.pop('levels');assert len(levels)==1 and t['width']==20 and t['height']==10 and t['format']=='I4'
    rgba=levels[0]['rgba'];assert len(rgba)==800 and sha256(rgba)=='561ab488c9d1f7dd7dcef8b40fd148bcfe4362a3c76554194225b65dc8bbe4f4'
    a.out.mkdir(parents=True,exist_ok=True);name='trail_texture_41_mip_0.rgba';(a.out/name).write_bytes(rgba)
    record={'schema':'smn64.trail.texture.v1','rom_sha256':ROM_SHA256,'lookup':'effect2 -> u16[800F51EC+2*2]','slot':slot,'stream_group':3,'stream_range':[start,end],'source_sha256':sha256(raw),'texture':t,'file':name,'rgba_sha256':sha256(rgba)}
    (a.out/'trail-texture.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
if __name__=='__main__':main()
