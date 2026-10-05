"""Extract exact original dome/ring data from the user's pinned USA cartridge.

No game data is bundled. Outputs must remain in ignored private runtime storage.
The source decoder and algorithm license are documented in ../mesh/SOURCES.md.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from codex.spiderman.assets.archive import (ROM_SHA256, FormatError, load_rom,
    read_archive, require, sha256, table, u32)
from codex.spiderman.mesh.decode_spiderman import parse_render, texture, unpack

MODELS = {165: ('firedome', 40), 166: ('firering', 41), 226: ('ring', 227),
          248: ('webdome2', 269), 249: ('webdome3', 270)}
EXPECTED_RENDER = {
 165:'330aeec77b0ecfccfcd2e762891fc30b096da01eb652e4518d9bffc216d78f4c',
 166:'2b8b15731c49f01f6dca37c3164a29a2e3df0c23eb1a9fe9825759cdd132b397',
 226:'9fd90366d16d2101d3d88ca5da3eea7f78fa6ffd6cc40e186660f08a956009e5',
 248:'40b8791d8b6f62ccde63911229c441a64c4a9b71359c365c2202d4b96a31eff6',
 249:'a523463c04ee281f4844924fb01674044e7ff9eb8d89e12b7df20634972cf92d'}


def shell_objects(data):
    """Static 0x20004 object table and opaque authored chunks, no fake clips."""
    require(u32(data) == 0x20004, 'unsupported dome shell revision')
    meta,n=u32(data,4),u32(data,8)
    require(1<=n<=5 and 12+n*36+4 == meta, 'invalid dome object table')
    count=u32(data,12+n*36)
    require(count==n, 'dome mesh/object ownership mismatch')
    objects=[]
    for i in range(n):
        p=12+i*36
        objects.append({'object_index':i,'source_offset':p,'flags':u32(data,p),
            'bind_translation_fixed12':list(unpack('>3i',data,p+4)),
            'mesh_index':unpack('>H',data,p+22)[0],
            'source_record_sha256':sha256(data[p:p+36])})
    chunks=[];p=meta
    for _ in range(32):
        tag=u32(data,p)
        if tag==0xffffffff:
            p+=4;break
        length=u32(data,p+4);end=p+8+length
        require(end<=len(data), 'dome chunk exceeds shell')
        chunks.append({'tag':tag,'range':[p+8,end],'sha256':sha256(data[p+8:end])})
        p=end
    else:raise FormatError('unterminated dome chunk list')
    require(p+n*4+4<=len(data) and u32(data,p+n*4)==n, 'dome name/count table mismatch')
    require(data[p+n*4+4:] in (b'',b'\0'*4), 'dome shell padding mismatch')
    names=list(unpack('>'+str(n)+'I',data,p))
    require(sorted(x['mesh_index'] for x in objects)==list(range(n)),
            'dome object placement ambiguous')
    for x in objects:x['mesh_name_hash']=names[x['mesh_index']]
    return objects,chunks


def records(rom_path):
    archive=read_archive(load_rom(rom_path))
    groups={g['index']:g['data'] for g in archive['groups']}
    models=[]
    for slot,(name,render_slot) in MODELS.items():
        require(archive['names'][slot]['name']==name,'original dome name mismatch')
        a,b=table(groups[0])[slot];bundle=groups[0][a:b];parts=table(bundle)
        require(len(parts)==4,'dome bundle shape mismatch')
        sa,sb=parts[2];ra,rb=parts[3]
        require(rb-ra==4 and u32(bundle,ra)==render_slot,'dome render slot mismatch')
        r0,r1=table(groups[2])[render_slot];render=groups[2][r0:r1]
        require(sha256(render)==EXPECTED_RENDER[slot],'dome render identity mismatch')
        objects,chunks=shell_objects(bundle[sa:sb])
        meshes,skipped=parse_render(render)
        require(not skipped and len(meshes)==len(objects),'missing dome node')
        for mesh in meshes:
            require(mesh['consumed_vertices']==len(mesh['vertices']), 'unconsumed dome pool')
            require(mesh['has_normals'] is False and len(mesh['groups'])==1,
                    'unsupported dome material grouping')
            group=mesh['groups'][0]
            require(group['texture_slot']==(402 if slot==165 and mesh['node_index']==1 else 403 if slot in (165,248,249) else 404), 'dome texture mismatch')
            require(all(c[3]==0 for t in mesh['triangles'] for c in t['corners']),
                    'dome uses unsupported matrix palette')
            start,end=mesh['source_range'];children=table(render,start,end)
            pa,pb=children[2];pool=render[pa+8:pb]
            require(len(pool)==16*len(mesh['vertices']), 'unexpected pool trailing bytes')
            mesh['source_vertex_pool_range']=[pa+8,pb]
            mesh['source_vertex_pool_sha256']=sha256(pool)
            mesh['runtime_vertices_be']=b''.join(bytes(pool[k*len(mesh['vertices'])+i]
                for k in range(16)) for i in range(len(mesh['vertices'])))
            mesh['packed_tokens']=render[slice(*group['token_range'])]
            mesh['runtime_vertex_sha256']=sha256(mesh['runtime_vertices_be'])
        models.append({'slot':slot,'name':name,'name_evidence':archive['names'][slot],
          'bundle_stream_range':[a,b],'bundle_sha256':sha256(bundle),
          'shell_bundle_range':[sa,sb],'shell_sha256':sha256(bundle[sa:sb]),
          'render_slot':render_slot,'render_stream_range':[r0,r1],
          'render_sha256':sha256(render),'objects':objects,'chunks':chunks,
          'meshes':meshes,'render_raw':render})
    textures=[]
    for slot in (402,403,404):
        a,b=table(groups[3])[slot];raw=groups[3][a:b];decoded=texture(raw)
        require(decoded['undecoded_payload_bytes']==0 and len(decoded['levels'])==1,
                'unsupported dome texture payload')
        textures.append(decoded|{'slot':slot,'source_range':[a,b],
            'source_sha256':sha256(raw),'raw':raw})
    return {'rom_sha256':ROM_SHA256,'boot_sha256':sha256(archive['boot']),
      'boot_load_base':archive['boot_load_base'],'models':models,'textures':textures}


def export(rom_path,out):
    data=records(rom_path);out=Path(out);out.mkdir(parents=True,exist_ok=True)
    manifest={'schema':'n64codexlab.spiderman.dome.v1',
        **{k:v for k,v in data.items() if k not in ('models','textures')},
        'models':[],'textures':[]}
    for model in data['models']:
        m={k:v for k,v in model.items() if k not in ('meshes','render_raw')};m['meshes']=[]
        for mesh in model['meshes']:
            node=mesh['node_index'];stem=f"{model['name']}_{node}"
            v=mesh['runtime_vertices_be'];tokens=mesh['packed_tokens']
            # Native-endian independent source representations. Runtime Vtx bytes
            # preserve flags, position, original ST and all four authored RGBA bytes.
            (out/f'{stem}.vtxbe').write_bytes(v)
            corners=[]
            for t in mesh['triangles']:
                for pool,s,tv,matrix in t['corners']:
                    corners.append(struct.pack('>HhhH',pool,s,tv,matrix))
            corner_bytes=b''.join(corners)
            (out/f'{stem}.cornersbe').write_bytes(corner_bytes)
            x={k:v for k,v in mesh.items() if k not in
               ('runtime_vertices_be','packed_tokens')}
            x.update({'vertex_file':f'{stem}.vtxbe','corner_file':f'{stem}.cornersbe',
                'corner_sha256':sha256(corner_bytes),'packed_token_sha256':sha256(tokens)})
            m['meshes'].append(x)
        manifest['models'].append(m)
    for tex in data['textures']:
        t={k:v for k,v in tex.items() if k not in ('levels','raw')};t['levels']=[]
        for level,item in enumerate(tex['levels']):
            filename=f"texture_{tex['slot']}_{level}.rgba";pixels=item['rgba']
            (out/filename).write_bytes(pixels)
            t['levels'].append({k:v for k,v in item.items() if k!='rgba'}|
                {'file':filename,'sha256':sha256(pixels)})
        manifest['textures'].append(t)
    (out/'dome_assets.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom')
    p.add_argument('--out',default='codex/.runtime/spiderman-dome-assets')
    args=p.parse_args();m=export(args.rom,args.out)
    print(json.dumps({'models':[{k:v for k,v in x.items() if k in ('slot','name','render_slot')}
        for x in m['models']],'textures':[{k:v for k,v in x.items() if k in
        ('slot','name','width','height','format')} for x in m['textures']]},indent=2))
