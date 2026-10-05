"""Extract original strand/impact textures into ignored private runtime storage."""
import argparse,json,struct
from pathlib import Path
from codex.spiderman.assets.archive import load_rom,read_archive,table,sha256,ROM_SHA256
from codex.spiderman.mesh.decode_spiderman import texture

def main():
    p=argparse.ArgumentParser();p.add_argument('--rom',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    if '.runtime' not in a.out.resolve().parts:raise SystemExit('Output must be inside ignored .runtime storage')
    arc=read_archive(load_rom(a.rom));boot=arc['boot'];base=arc['boot_load_base'];stream=arc['groups'][3]['data'];entries=table(stream)
    knot=struct.unpack_from('>H',boot,0x800f51f4-base)[0];assert knot==46
    matches=[]
    for i in range(82):
        key,slot=struct.unpack_from('>II',boot,0x800f5210-base+8*i)
        if key==0x3af6dff3:matches.append((i,slot))
    assert matches[0]==(6,47)
    a.out.mkdir(parents=True,exist_ok=True);records=[]
    for slot,role in [(knot,'webknot particle'),(matches[0][1],'zip target websplat')]:
        start,end=entries[slot];raw=stream[start:end];t=texture(raw);levels=[]
        for i,level in enumerate(t.pop('levels')):
            name=f'web_texture_{slot}_mip_{i}.rgba';(a.out/name).write_bytes(level['rgba'])
            levels.append({k:v for k,v in level.items() if k!='rgba'}|{'file':name,'sha256':sha256(level['rgba'])})
        records.append(t|{'slot':slot,'role':role,'stream_group':3,'stream_range':[start,end],'source_sha256':sha256(raw),'levels':levels})
    result={'schema':'smn64.web.textures.v1','rom_sha256':ROM_SHA256,'texture_stream_sha256':sha256(stream),'knot_lookup':'800B1738(effect4)->8006A228->u16[800F51F4]=46','splat_lookup':'800AE358(hash3AF6DFF3)->80069E04 first match800F5240=47; later duplicate59 not selected','textures':records}
    (a.out/'web-textures.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
