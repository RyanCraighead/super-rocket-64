"""Original N64 THPS1 mesh, texture, hierarchy and clip extraction.

See SOURCES.md for MIT algorithm provenance and limitations. Runtime outputs
contain user-supplied game assets and must remain outside version control.
"""
from __future__ import annotations
import argparse
import json
import math
import struct
from pathlib import Path
from codex.thps.assets.archive import FormatError,require,u32,table,sha256,load_rom,read_archive,ROM_SHA256

def unpack(fmt,data,p=0):
    size=struct.calcsize(fmt)
    require(0<=p<=len(data)-size,'field out of bounds')
    return struct.unpack_from(fmt,data,p)

def parse_shell(data):
    require(u32(data) in (0x20003,0x20004,0x20006),'unsupported N64 shell')
    meta,n=u32(data,4),u32(data,8)
    require(1<=n<=4096 and 12+n*36+4 <= meta <= len(data)-4,'invalid shell object table')
    objects=[]
    for i in range(n):
        p=12+i*36
        objects.append({'index':i,'flags':u32(data,p),'bind_translation_fixed12':list(unpack('>3i',data,p+4)),'mesh_index':unpack('>H',data,p+22)[0],'parent':-1,'source_offset':p})
    mesh_count=u32(data,12+n*36)
    require(1<=mesh_count<=65535,'invalid mesh count')
    chunks=[]; p=meta
    for _ in range(64):
        tag=u32(data,p)
        if tag==0xffffffff:
            p+=4; break
        length=u32(data,p+4); end=p+8+length
        require(end<=len(data),'chunk exceeds shell')
        chunks.append({'tag':tag,'start':p+8,'end':end})
        if tag==0x52454948:
            require(length in (2*n,(2*n+3)&~3),'hierarchy must cover every object plus optional alignment padding')
            for i,parent in enumerate(unpack('>'+str(n)+'H',data,p+8)):
                require(parent<n,'hierarchy parent out of bounds')
                objects[i]['parent']=-1 if parent==i else parent
        p=end
    else:
        raise FormatError('unterminated shell chunk chain')
    require(p+4*mesh_count<=len(data),'mesh-name table truncated')
    names=list(unpack('>'+str(mesh_count)+'I',data,p))
    for o in objects:
        require(o['mesh_index']<mesh_count,'object selects invalid mesh')
        o['mesh_name_hash']=names[o['mesh_index']]
        seen=set(); j=o['index']
        while j!=-1:
            require(j not in seen,'hierarchy cycle')
            seen.add(j); j=objects[j]['parent']
    banks=[c for c in chunks if c['tag'] in (0x2a,0x2c)]
    require(banks,'missing animation bank')
    return {'objects':objects,'mesh_count':mesh_count,'chunks':chunks,'animation_chunk':banks[-1],'mesh_name_hashes':names}

def parse_render(data):
    meshes=[]; skipped=[]
    for node,(start,end) in enumerate(table(data)):
        children=table(data,start,end)
        require(len(children)==3,'render node needs three children')
        (ba,bb),(ga,gb),(pa,pb)=children
        bounds=list(unpack('>'+str((bb-ba)//4)+'f',data,ba))
        require(len(bounds)>=6 and all(math.isfinite(v) for v in bounds),'invalid mesh bounds')
        count=u32(data,pa)
        if count==0:
            skipped.append({'node':node,'reason':'empty vertex pool'}); continue
        require(u32(data,pa+4)==0 and 1<=count<=65535 and 8+16*count<=pb-pa,'invalid vertex pool')
        body=data[pa+8:pa+8+16*count]
        def decode(transposed):
            verts=[]
            for i in range(count):
                rec=bytes(body[k*count+i] for k in range(16)) if transposed else body[i*16:i*16+16]
                xyz=unpack('>3h',rec); st=unpack('>2h',rec,8)
                verts.append(list(xyz)+list(st)+list(rec[12:16]))
            return verts
        def error(v):
            extrema=[min(x[c] for x in v) for c in range(3)]+[max(x[c] for x in v) for c in range(3)]
            return sum((a-b)**2 for a,b in zip(extrema,bounds))
        plain,trans=decode(False),decode(True); ep,et=error(plain),error(trans)
        require(ep!=et or plain==trans,'THPS1 vertex layout ambiguous')
        vertices=trans if et<ep else plain; cache=[None]*32; cursor=0; triangles=[]; groups=[]; normals=None
        for gi,(a,b) in enumerate(table(data,ga,gb)):
            c=table(data,a,b)
            require(len(c)==3 and c[0][1]-c[0][0]==12,'invalid geometry group')
            desc=c[0][0]; kind=unpack('>H',data,desc+6)[0]
            group={'index':gi,'descriptor_offset':desc,'kind':kind,'token_range':list(c[1]),'face_flag_range':list(c[2])}
            if kind&0x8800:
                group['skipped']=True; groups.append(group); continue
            lit=not bool(kind&0x400)
            require(normals is None or normals==lit,'mixed normal/color mesh requires per-group handling')
            normals=lit
            slot=u32(data,desc) if kind&1 else 0
            fa,fb=c[2]; faces=u32(data,fa)
            require(fa+4+4*faces<=fb,'truncated face flags')
            flags=list(unpack('>'+str(faces)+'I',data,fa+4))
            tokens=data[c[1][0]:c[1][1]]; p=0; matrix=0; fi=0; ended=False
            while p<len(tokens):
                op=tokens[p]
                if op==0:
                    ended=True; break
                if op&0x80:
                    require(p+2<=len(tokens),'truncated triangle token')
                    word=(op<<8)|tokens[p+1]; idx=[(word>>10)&31,(word>>5)&31,word&31]
                    corners=[cache[s] for s in idx]
                    require(all(x is not None for x in corners),'triangle uses unloaded vertex')
                    require(fi<len(flags),'missing triangle flags')
                    triangles.append({'corners':[list(x) for x in corners],'flags':flags[fi],'texture_slot':slot,'group_index':gi})
                    fi+=1;p+=2
                elif op&0xe0==0x20:
                    require(p+2<=len(tokens),'truncated vertex token')
                    word=(op<<8)|tokens[p+1]; n=word&31 or 32; dst=(word>>5)&31
                    require(dst+n<=32 and cursor+n<=len(vertices),'vertex load exceeds cache or pool')
                    for k in range(n):
                        v=vertices[cursor]
                        cache[dst+k]=(cursor,v[3],v[4],matrix);cursor+=1
                    p+=2
                elif op&0xe0==0x40:
                    require(p+2<=len(tokens),'truncated matrix token')
                    matrix=tokens[p+1];p+=2
                elif op&0xe0==0x60:
                    require(p+5<=len(tokens),'truncated UV token')
                    dst=op&31; require(cache[dst] is not None,'UV changes unloaded vertex')
                    v,_,_,m=cache[dst]; s,t=unpack('>2h',tokens,p+1);cache[dst]=(v,s,t,m);p+=5
                else:
                    raise FormatError(f'unknown packed display-list opcode {op:#x}')
            require(ended,'unterminated packed display list')
            require(fi==faces,'face flags do not match triangle count')
            group.update({'texture_slot':slot,'has_normals':lit,'triangle_count':fi,'skipped':False});groups.append(group)
        meshes.append({'node_index':node,'source_range':[start,end],'bounds':bounds,'has_normals':normals,'vertices':vertices,'triangles':triangles,'groups':groups,'layout_error':{'plain':ep,'transposed':et},'consumed_vertices':cursor})
    return meshes,skipped

def rgba5551(v):
    return [(v>>11&31)*255//31,(v>>6&31)*255//31,(v>>1&31)*255//31,(v&1)*255]

def texture(data):
    require(len(data)>=64,'truncated texture header')
    nul=data.find(b'\0',0,32)
    require(2<=nul<32 and all(32<=c<=126 for c in data[:nul]),'invalid texture name')
    name=data[:nul].decode('ascii'); w,h=unpack('>2H',data,32); word=unpack('>H',data,38)[0]; size=unpack('>H',data,42)[0]
    fmt,bpp=word>>8,16 if word&255>=16 else word&255
    require(1<=w<=1024 and 1<=h<=1024 and (fmt,bpp) in ((0,16),(2,4),(3,4),(3,8),(4,4),(4,8)),'unsupported texture dimensions/format')
    require(63+size+(32 if fmt==2 else 0)<=len(data),'texture payload truncated')
    def stride(w):return ((w*bpp+7)//8+7)&~7
    require(stride(w)*h<=size,'texture exceeds declared data size')
    layouts=[];o=0;lw,lh=w,h
    while o+stride(lw)*lh<=size:
        layouts.append((lw,lh,stride(lw),o));o+=stride(lw)*lh
        if o==size or lw==lh==1 or word==0x14:break
        lw,lh=max(1,lw//2),max(1,lh//2)
    if o!=size or word==0x14:
        layouts=layouts[:1]
    levels=[]
    for lw,lh,s,o in layouts:
        pixels=data[63+o:63+o+s*lh]; out=bytearray()
        for y in range(lh):
            for x in range(lw):
                tx=x^((32//bpp) if y&1 else 0); pos=y*s+tx*bpp//8
                if bpp==16:rgba=rgba5551(unpack('>H',pixels,pos)[0])
                else:
                    v=pixels[pos]
                    if bpp==4:v=(v>>4) if tx%2==0 else (v&15)
                    if fmt==2:rgba=rgba5551(unpack('>H',data,63+size+v*2)[0])
                    elif fmt==3:
                        intensity=(v>>1)*255//7 if bpp==4 else (v>>4)*17
                        alpha=(v&1)*255 if bpp==4 else (v&15)*17
                        rgba=[intensity]*3+[alpha]
                    else:
                        intensity=v*17 if bpp==4 else v; rgba=[intensity]*4
                out.extend(rgba)
        levels.append({'width':lw,'height':lh,'rgba':bytes(out),'payload_offset':63+o,'payload_bytes':s*lh})
    flags=u32(data,47)
    return {'name':name,'width':w,'height':h,'format_word':word,'format':{(0,16):'RGBA16',(2,4):'CI4',(3,4):'IA4',(3,8):'IA8',(4,4):'I4',(4,8):'I8'}[(fmt,bpp)],'wrap_s':data[44],'wrap_t':data[45],'alpha_threshold':data[46],'render_flags':flags,'render_class':('coverage' if flags&3==1 else 'translucent' if flags&3==3 else 'opaque') if data[46]==255 else 'custom_threshold','levels':levels,'undecoded_payload_bytes':size-sum(x['payload_bytes'] for x in levels),'has_auxiliary_plane':word==0x14}

def s16(n):return (n+32768)%65536-32768

def truncdiv(a,b):return (abs(a)//b)*(-1 if a<0 else 1)

def decode_channel(data,offset,frames):
    require(1<=frames<=4096 and 0<=offset<len(data),'invalid animation channel input')
    p=offset; head=data[p];p+=1; segments=(head>>4)+1; mode=head&15
    if mode==15:return [0]*frames,p
    require(p+2<=len(data),'truncated animation initial sample')
    prev=unpack('<h',data,p)[0];p+=2
    if mode==14:return [prev]*frames,p
    out=[prev]; full=(frames-1)//segments; remainder=frames-(full*segments+1); bitpos=p*8
    for span in [segments]*full+([remainder] if remainder else []):
        if mode==0:
            require(p+2<=len(data),'truncated animation endpoint')
            endpoint=unpack('<h',data,p)[0];p+=2; delta=endpoint-prev
        else:
            bits=mode+1
            require(bitpos+bits<=len(data)*8,'animation delta crosses clip bound')
            value=0
            for i in range(bits):
                value=value*2+((data[(bitpos+i)//8]>>(7-(bitpos+i)%8))&1)
            bitpos+=bits
            delta=value-(1<<bits) if value&(1<<(bits-1)) else value
            endpoint=s16(prev+delta)
        step=s16(truncdiv(delta,span))
        for _ in range(span-1):
            prev=prev+step
            if mode==0:prev=s16(prev)
            out.append(s16(prev))
        prev=endpoint;out.append(endpoint)
    require(len(out)==frames,'animation sample count mismatch')
    return out,p if mode==0 else (bitpos+7)//8

def decode_clips(data,shell):
    c=shell['animation_chunk']; require(c['tag']==0x2c,'this player importer supports compressed0x2c only')
    bank=data[c['start']:c['end']]; count=u32(bank);require(1<=count<=4096 and 4+count*8<=len(bank),'invalid animation entry count')
    entries=[]
    for i in range(count):
        off,zero,frames=unpack('>IHH',bank,4+i*8)
        require(zero==0 and 1<=frames<=4096 and 4+count*8<=off<len(bank),'invalid animation table entry')
        require(not entries or off>entries[-1][0],'animation payload offsets not increasing')
        entries.append((off,frames))
    clips=[];bones=len(shell['objects'])
    require(1<=bones<=4096 and sum(f for _,f in entries)*bones*6 <= 8*1024*1024, 'animation decoded-sample budget exceeded')
    for slot,(off,frames) in enumerate(entries):
        end=entries[slot+1][0] if slot+1<len(entries) else len(bank);payload=bank[off:end]
        info={'slot':slot,'name':f'anim_{slot}','frame_count':frames,'bone_count':bones,'source_shell_range':[c['start']+off,c['start']+end],'source_sha256':sha256(payload),'source_bytes':len(payload),'frame_rate':None,'loop_mode':None,'action_role':None}
        try:
            channels=[];p=0
            for bone in range(bones):
                bc=[]
                for channel in range(6):
                    values,p=decode_channel(payload,p,frames);bc.append(values)
                channels.append(bc)
            info.update({'status':'decoded','consumed_bytes':p,'trailing_bytes':len(payload)-p,'frames':[[[channels[b][ch][f] for ch in range(6)] for b in range(bones)] for f in range(frames)]})
            packed=struct.pack('<'+'h'*(frames*bones*6),*(v for f in info['frames'] for b in f for v in b))
            info['decoded_s16le_sha256']=sha256(packed)
        except FormatError as e:
            info.update({'status':'failed','error':str(e)})
        clips.append(info)
    return clips

def write_json(path,data):
    path.write_text(json.dumps(data,separators=(',',':'))+'\n')

def export(rom_path,out):
    out=Path(out);rom=load_rom(rom_path); archive=read_archive(rom,out)
    named=[slot for slot,v in archive['names'].items() if v['name'].lower()=='hawk']
    require(len(named)==1,'exact hawk model name is absent or ambiguous'); slot=named[0]
    groups={g['index']:g for g in archive['groups']}
    # The source-locked USA cartridge directory roles are verified by container shape below.
    models=groups[0]['data']; a,b=table(models)[slot];bundle=models[a:b];parts=table(bundle)
    require(len(parts)==4,'expected four-part model bundle')
    (oa,ob),(ba,bb),(sa,sb),(ra,rb)=parts
    require(rb-ra==4,'invalid render-bank reference');render_id=u32(bundle,ra)
    shell_data=bundle[sa:sb]; shell=parse_shell(shell_data)
    render_group=groups[2]['data'];r0,r1=table(render_group)[render_id];render_data=render_group[r0:r1]
    meshes,skipped=parse_render(render_data); by_node={m['node_index']:m for m in meshes}
    placed=set();vertices=[];triangles=[];materials=[];material_ids={};invisible=0
    for obj in shell['objects']:
        if obj['mesh_index'] not in by_node:continue
        mesh=by_node[obj['mesh_index']]
        require(mesh['node_index'] not in placed,'ambiguous repeated node placement');placed.add(mesh['node_index'])
        for tri in mesh['triangles']:
            flags=tri['flags']; runtime=flags if flags&0x40 else flags^0x80
            if runtime&0xc0==0:invisible+=1;continue
            key=(tri['texture_slot'],flags,mesh['has_normals'])
            if key not in material_ids:
                material_ids[key]=len(materials);materials.append({'texture_slot':key[0],'face_flags':key[1],'has_normals':key[2],'double_sided':bool(flags&0x200),'semi_transparent':bool(flags&0x40),'blend_rate':flags>>7&3})
            indices=[]
            for index,s,t,matrix in tri['corners']:
                require(0<=matrix<len(shell['objects']),'corner matrix outside original joint palette')
                v=mesh['vertices'][index]
                indices.append(len(vertices));vertices.append({'position':v[:3],'st':[s,t],'rgba':v[5:9],'matrix':matrix,'node':mesh['node_index'],'pool_vertex':index})
            triangles.append(indices+[material_ids[key]])
    texture_stream=groups[3]['data'];texture_ranges=table(texture_stream);textures=[]
    texture_dir=out/'textures';texture_dir.mkdir(exist_ok=True)
    for ts in sorted({m['texture_slot'] for m in materials if m['texture_slot']}):
        require(ts<len(texture_ranges),'texture slot outside dictionary');t0,t1=texture_ranges[ts];raw=texture_stream[t0:t1];tex=texture(raw)
        levels=[]
        for level,item in enumerate(tex.pop('levels')):
            path=f'textures/slot_{ts}_mip_{level}.rgba';(out/path).write_bytes(item['rgba'])
            levels.append({k:v for k,v in item.items() if k!='rgba'}|{'file':path,'sha256':sha256(item['rgba'])})
        textures.append(tex|{'slot':ts,'levels':levels,'source_stream_range':[t0,t1],'source_sha256':sha256(raw)})
    clips=decode_clips(shell_data,shell);clip_dir=out/'animations';clip_dir.mkdir(exist_ok=True);animation_refs=[]
    for clip in clips:
        path=f"animations/anim_{clip['slot']:03}.json";write_json(out/path,clip)
        animation_refs.append({k:v for k,v in clip.items() if k!='frames'}|{'file':path})
    model={'schema':'n64codexlab.thps.original.v1','source':{'rom_sha256':ROM_SHA256,'rom_byte_order':'big_endian','model_name':'hawk','model_slot':slot,'name_evidence':archive['names'][slot],'bundle_stream_group':0,'bundle_stream_range':[a,b],'shell_bundle_range':[sa,sb],'shell_sha256':sha256(shell_data),'render_group':2,'render_slot':render_id,'render_stream_range':[r0,r1],'render_sha256':sha256(render_data),'decoder_source_commit':'3c9028e0178deb9060190362d1c47039a32846e2'},'coordinate_contract':{'axis':'native PSX/N64 (Y down); viewer may reflect Y,Z','unit_system':'upstream normalized display units, not native game-world collision units','vertex_multiplier':8,'world_divisor':36,'bind_translation_fixed_divisor':4096,'bind_translation_world_divisor':2.25,'angle_units_per_turn':4096,'rotation_order':'Ry*Rx*Rz column vectors','rotation_binding':'piecewise absolute, not inherited','translation_binding':'parent_rotation * raw_translation + parent_world_translation','matrix_binding':'global G_MTX','texture_st_divisor':32,'texture_half_texel_bias':16,'animation_timing':'unknown','native_matrix_translation_multiplier':8,'native_pose_translation_channel_multiplier':0.125,'posed_vertex_to_source_world':0.5,'composed_matrix_policy':'source-derived float preview; original N64 float32 and quantized output under verification'},'objects':shell['objects'],'vertices':vertices,'triangles':triangles,'materials':materials,'textures':textures,'animations':animation_refs,'light_rig':archive['light'],'counts':{'objects':len(shell['objects']),'mesh_nodes':len(meshes),'raw_pool_vertices':sum(len(m['vertices']) for m in meshes),'decoded_triangles':sum(len(m['triangles']) for m in meshes),'visible_triangles':len(triangles),'invisible_triangles':invisible,'render_vertices':len(vertices),'textures':len(textures),'animation_slots':len(clips),'decoded_clips':sum(c['status']=='decoded' for c in clips),'decoded_frames':sum(c['frame_count'] for c in clips if c['status']=='decoded')},'render_evidence':[{k:v for k,v in m.items() if k not in ('vertices','triangles')} for m in meshes],'skipped_nodes':skipped,'limitations':['Numeric animation slots have no inferred action names, cadence, loop, or blend behavior','Pose matrices are a source-derived floating-point preview until matching original N64 execution is verified','No original gameplay mechanics or controller is supplied by asset extraction','Original N64 rasterizer, texture filtering, coplanar draw order, alpha coverage and dynamic lighting are not reproduced']}
    write_json(out/'thps_model.json',model)
    write_json(out/'archive.json',{k:v for k,v in archive.items() if k not in ('boot','groups')}|{'groups':[{k:v for k,v in g.items() if k!='data'} for g in archive['groups']]})
    for filename,blob in [('hawk.psx.n64',shell_data),('renderbank.bin',render_data),('objects.bin',bundle[oa:ob]),('bounds.bin',bundle[ba:bb])]:
        (out/filename).write_bytes(blob)
    print(json.dumps(model['counts'],indent=2))
    return model

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom');p.add_argument('--out',default='codex/.runtime/thps-assets');args=p.parse_args();export(args.rom,args.out)
