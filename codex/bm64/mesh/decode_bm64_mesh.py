#!/usr/bin/env python3
"""Bounded original BM64 US 1.0 F3DEX mesh/CI8 texture conversion.

Input ROM is read only and authenticated. No ROM bytes or generated assets belong
in version control. This module does not invoke a third-party executable.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'bm64' / 'assets'))
# parents[2] is codex; imports resolve within the audited repository.
from prepare_bm64 import read_rom, validate, decode_lzss, InputError, EXPECTED_SHA1

SCHEMA = 'bm64-original-mesh-v1'
MAX_MODEL = 1024 * 1024
MAX_COMMANDS = 20000
MAX_VERTICES = 10000
MAX_TRIANGLES = 10000
MAX_TEXTURE_PIXELS = 1024 * 1024


class DecodeError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise DecodeError(message)


def read(data, offset, size, alignment=1):
    require(type(offset) is int and type(size) is int and offset >= 0 and size >= 0,
            'invalid source range')
    require(offset % alignment == 0 and offset <= len(data) and size <= len(data) - offset,
            f'misaligned or out-of-bounds source read {offset:x}+{size:x}')
    return data[offset:offset + size]


def entries(data):
    require(isinstance(data, bytes) and 12 <= len(data) <= MAX_MODEL, 'invalid model size')
    require(data[:4] == b'64\x008' and data[8:12] == b'\x02'*4, 'unsupported BM64 model signature')
    count, = struct.unpack_from('>I', data, 4)
    require(1 <= count <= 256, 'model entry count out of range')
    read(data, 12, count * 12)
    result = []
    for index in range(count):
        kind, param, offset = struct.unpack_from('>III', data, 12 + index*12)
        # Type4 encodes segment assignments in the offset word; it is not a pointer.
        if kind != 4:
            require(offset <= len(data), 'model entry pointer outside model')
        result.append(dict(index=index, type=kind, param=param, offset=offset))
    return result


def extract_model(rom, model_id):
    require(model_id in (0, 1, 2, 3, 4, 5, 6, 16, 17, 0x49, 0x4a), 'model outside verified extraction profile')
    require(len(rom) == 0x800000 and hashlib.sha1(rom).hexdigest() == EXPECTED_SHA1,
            'model extraction requires authenticated original US 1.0 ROM')
    # Runtime 80225E2C reads 256-entry table pages beginning at bank+8.
    # The historical FileRipper starts at +0x10 and therefore numbers file0 as -1.
    base = 0x300000
    data_offset, table_count = struct.unpack_from('>II', rom, base)
    require((data_offset, table_count) == (0x2008, 0x400), 'unexpected original model bank header')
    relative, size = struct.unpack_from('>II', rom, base + 8 + model_id*8)
    require(relative != 0xffffffff and 4 <= size <= MAX_MODEL, 'invalid model bank entry')
    stored = read(rom, base + data_offset + relative, size)
    decoded_size, = struct.unpack_from('>I', stored)
    decoded = decode_lzss(stored, decoded_size)
    entries(decoded)
    return decoded, dict(model_id=model_id, table_rom=base + 8 + model_id*8,
                         rom_offset=base + data_offset + relative, stored_size=size,
                         decoded_size=len(decoded), sha256=hashlib.sha256(decoded).hexdigest())


def tile_state():
    return dict(fmt=0,size=2,line=0,tmem=0,palette=0,cm_s=0,cm_t=0,
                mask_s=0,mask_t=0,shift_s=0,shift_t=0,uls=0,ult=0,lrs=0,lrt=0)


def rgba16(v):
    rgb = [(v >> s) & 31 for s in (11, 6, 1)]
    return [(x << 3) | (x >> 2) for x in rgb] + [255 if v & 1 else 0]


class Memory:
    def __init__(self, model, texture_model=None):
        self.segments = {2: model}
        self.texture_catalog = {}
        self.palette_catalog = {}
        if texture_model is not None:
            es = entries(texture_model)
            # Original player assigns the first twelve entries to slots0..11;
            # draw 8022D414 binds these at RSP segment IDs3..14.
            require(len(es) >= 12, 'missing original player texture assignments')
            for i, e in enumerate(es[:12]):
                seg = i+3
                off=e['offset']
                if e['type'] == 21:
                    w,h=e['param'] >> 16,e['param'] & 65535
                    require(0 < w <= 1024 and 0 < h <= 1024 and w*h <= MAX_TEXTURE_PIXELS,
                            'invalid texture catalog dimensions')
                    self.segments[seg] = read(texture_model, off, w*h)
                    self.texture_catalog[seg << 24] = (w,h)
                elif e['type'] == 26:
                    require(0 < e['param'] <= 256, 'invalid palette catalog length')
                    self.segments[seg] = read(texture_model, off, e['param']*2)
                    self.palette_catalog[seg << 24] = e['param']
                else:
                    raise DecodeError('unsupported player texture entry')

    def read(self, address, count, alignment=1):
        require(type(address) is int and 0 <= address <= 0xffffffff, 'invalid segmented pointer')
        seg,off=address>>24,address&0xffffff
        require(seg in self.segments, f'unbound segment {seg:02x}')
        return read(self.segments[seg], off, count, alignment)


class Decoder:
    """F3DEX1 source interpreter; unsupported commands fail closed."""
    def __init__(self, memory):
        self.memory = memory
        self.cache = [None]*32
        self.vertices=[]; self.triangles=[]; self.materials=[]
        self.vertex_ids={};self.material_ids={};self.texture_payloads={}
        self.commands=0;self.opcodes={};self.warnings=set()
        # Original F3DEX1 initialization: 8029F6E8 sets0x204,80227634 adds0x22001.
        self.geometry=0x22205 # SHADE/LIGHTING/SMOOTH/CULL_BACK/ZBUFFER
        self.combine=[0,0];self.prim_color=[255]*4;self.env_color=[255]*4
        self.texture_enabled=False;self.texture_scale=[65535]*2;self.texture_tile=0
        self.tiles=[tile_state() for _ in range(8)]
        # Original nonfog caller defaults: static8029F718 plus80227658.
        # Scene-dependent color dither is recorded as the static default.
        self.image=None;self.tlut=None;self.tmem={};self.othermode_h=0x82c30;self.othermode_l=0x553078
        self.light_diffuse=[255]*3;self.light_ambient=[127]*3
        self.joint=0;self.billboard=False;self.root=None

    def texture(self, tile):
        if not self.texture_enabled:
            return None
        # SHADE-only combine does not sample TMEM despite stale texture enable.
        if self.combine == [0xffffff,0xfffe7d3e]:
            return None
        source=self.tmem.get(tile['tmem'])
        require(source is not None,'texture primitive reads unloaded TMEM')
        require(tile['fmt']==2 and tile['size'] in (0,1),'profile only supports original CI4/8 materials')
        require(self.tlut is not None and ((self.othermode_h>>14)&3)==2,'CI texture requires RGBA16 palette')
        require(tile['palette']==0,'profile palette bank must be zero')
        uls,ult,lrs,lrt=(tile[k] for k in ('uls','ult','lrs','lrt'))
        require(all(v%4==0 for v in (uls,ult,lrs,lrt)),'fractional render tile origin unsupported')
        w,h=(lrs-uls)//4+1,(lrt-ult)//4+1
        require(0 < w <= 1024 and 0 < h <= 1024 and w*h <= MAX_TEXTURE_PIXELS,'invalid tile size')
        image=source['image'];address=image['address']
        if source['kind']=='tile':
            stride=image['width'];x,y=uls//4,ult//4
            if tile['size']==0:
                # gDPLoadTextureTile_4b uploads packed nibbles through an 8-bit
                # image/tile, halves S (including the fractional endpoint),
                # then reinterprets TMEM through a CI4 render tile.
                require(source['rect']==(uls//2,ult,lrs//2,lrt) and x%2==0 and w%2==0,
                        'unsupported packed CI4 upload rectangle')
                require(x//2+w//2<=stride,'CI4 tile exceeds source row')
                packed=b''.join(self.memory.read(address+(y+row)*stride+x//2,w//2) for row in range(h))
                raw=bytes((packed[i//2] >> (4 if i%2==0 else 0)) & 15 for i in range(w*h))
            else:
                require(source['rect']==(uls,ult,lrs,lrt),'render tile does not match uploaded rectangle')
                require(x+w<=stride,'tile upload exceeds source row')
                raw=b''.join(self.memory.read(address+(y+row)*stride+x,w) for row in range(h))
        else:
            require(uls==0 and ult==0,'block tile origin must be zero')
            byte_count=(w*h*(4 << tile['size'])+7)//8
            require(byte_count <= source['loaded_bytes'],'texture exceeds loaded block')
            packed=self.memory.read(address,byte_count)
            raw=(bytes((packed[i//2] >> (4 if i%2==0 else 0)) & 15 for i in range(w*h))
                 if tile['size']==0 else packed)
        paladdr=self.tlut['address']
        available=self.memory.palette_catalog.get(paladdr,self.tlut['count'])
        require(all(index < available for index in raw),'CI index exceeds actual original palette')
        # Some original palettes contain fewer than256 entries though F0 loads256.
        # Read exactly the used entries, never unrelated bytes beyond that palette.
        pal=self.memory.read(paladdr,(max(raw)+1)*2)
        rgba=bytes(c for index in raw for c in rgba16(struct.unpack_from('>H',pal,index*2)[0]))
        name='tex_'+hashlib.sha256(rgba+struct.pack('>II',w,h)).hexdigest()[:24]+'.rgba'
        if name not in self.texture_payloads:
            require(len(self.texture_payloads)<256 and sum(map(len,self.texture_payloads.values()))+len(rgba)<=64*1024*1024,
                    'texture output budget exceeded')
        self.texture_payloads[name]=rgba
        return dict(rgba_file='textures/'+name,width=w,height=h,address=address,
                    tlut_address=paladdr,binding=None,format=2,size=tile['size'],
                    crop_origin=[uls//4,ult//4],source_width=image['width'] if source['kind']=='tile' else w)

    def material(self):
        tile=copy.deepcopy(self.tiles[self.texture_tile])
        mat=dict(geometry_mode=self.geometry,combine=self.combine[:],prim_color=self.prim_color[:],
                 env_color=self.env_color[:],texture_enabled=self.texture_enabled,
                 texture_scale=self.texture_scale[:],tile=tile,texture=self.texture(tile),
                 other_mode_h=self.othermode_h,other_mode_l=self.othermode_l,
                 light_diffuse=self.light_diffuse[:],light_ambient=self.light_ambient[:])
        key=json.dumps(mat,sort_keys=True)
        if key not in self.material_ids:
            require(len(self.materials)<256,'material limit')
            self.material_ids[key]=len(self.materials);self.materials.append(mat)
        return self.material_ids[key]

    def vertex(self,v):
        key=json.dumps(v,sort_keys=True)
        if key not in self.vertex_ids:
            require(len(self.vertices)<MAX_VERTICES,'vertex output limit')
            self.vertex_ids[key]=len(self.vertices);self.vertices.append(v)
        return self.vertex_ids[key]

    def triangle(self,word):
        slots=[(word>>s)&255 for s in (16,8,0)]
        require(all(n%2==0 and n<64 for n in slots),'invalid triangle slots')
        vs=[self.cache[n//2] for n in slots]
        require(all(v is not None for v in vs),'triangle reads unloaded vertex')
        require(len(self.triangles)<MAX_TRIANGLES,'triangle limit')
        self.triangles.append(dict(indices=[self.vertex(v) for v in vs],material=self.material(),root=self.root))

    def run(self,address,joint,billboard,label):
        self.joint,self.billboard,self.root=joint,billboard,label
        self.cache=[None]*32
        self._list(address,[])

    def _list(self,address,active):
        require(len(active)<32 and address not in active,'display list recursion/cycle')
        active=active+[address]
        while True:
            require(self.commands<MAX_COMMANDS,'display command limit')
            w0,w1=struct.unpack('>II',self.memory.read(address,8,8));pc=address;address+=8
            op=w0>>24;self.commands+=1;self.opcodes[f'{op:02x}']=self.opcodes.get(f'{op:02x}',0)+1
            if op==0xb8:return
            if op==4:
                n=(w0>>10)&63;raw_first=(w0>>16)&255;first=raw_first//2
                require(raw_first%2==0 and 1<=n<=32 and first+n<=32 and (w0&1023)==n*16-1,
                        'invalid F3DEX vertex load')
                raw=self.memory.read(w1,n*16,8)
                for i in range(n):
                    x,y,z,flag,u,v,r,g,b,a=struct.unpack_from('>3hH2h4B',raw,i*16)
                    self.cache[first+i]=dict(position=[x,y,z],uv=[u,v],color_normal=[r,g,b,a],
                                            joint_index=self.joint,billboard=self.billboard,
                                            lit=bool(self.geometry&0x20000),uv_scale=self.texture_scale[:],uv_processed=False)
            elif op in (0xbf,0xb1):
                if op==0xb1:self.triangle(w0)
                self.triangle(w1)
            elif op==6:
                mode=(w0>>16)&255;require(mode in (0,1),'invalid list branch')
                self._list(w1,active)
                if mode:return
            elif op==0xbb:
                require((w0&255) in (0,1),'invalid texture enable')
                self.texture_enabled=bool(w0&255);self.texture_tile=(w0>>8)&7
                self.texture_scale=[w1>>16,w1&65535]
            elif op==0xbc:
                where=w0&255;offset=(w0>>8)&65535
                require(where==10 and offset in (0x20,0x24,0x40,0x44),'unsupported move-word')
                rgb=list(w1.to_bytes(4,'big'))[:3]
                if offset in (0x20,0x24):self.light_diffuse=rgb
                else:self.light_ambient=rgb
            elif op==0xb6:self.geometry&=~w1
            elif op==0xb7:self.geometry|=w1
            elif op in (0xba,0xb9):
                shift=(w0>>8)&255;length=w0&255
                require(0<length<=32 and shift+length<=32,'invalid othermode range')
                mask=((1<<length)-1)<<shift
                if op==0xba:self.othermode_h=(self.othermode_h&~mask)|(w1&mask)
                else:self.othermode_l=(self.othermode_l&~mask)|(w1&mask)
            elif op==0xfd:
                self.image=dict(address=w1,fmt=(w0>>21)&7,size=(w0>>19)&3,width=(w0&4095)+1)
            elif op==0xf5:
                self.tiles[(w1>>24)&7].update(fmt=(w0>>21)&7,size=(w0>>19)&3,line=(w0>>9)&511,
                    tmem=w0&511,palette=(w1>>20)&15,cm_t=(w1>>18)&3,mask_t=(w1>>14)&15,
                    shift_t=(w1>>10)&15,cm_s=(w1>>8)&3,mask_s=(w1>>4)&15,shift_s=w1&15)
            elif op==0xf2:
                self.tiles[(w1>>24)&7].update(uls=(w0>>12)&4095,ult=w0&4095,lrs=(w1>>12)&4095,lrt=w1&4095)
            elif op in (0xf3,0xf4):
                require(self.image is not None,'texture upload missing source');tile=self.tiles[(w1>>24)&7]
                source=dict(image=self.image.copy(),kind='tile' if op==0xf4 else 'block')
                if op==0xf4:
                    require(self.image['size']==tile['size']==1,'unsupported tiled upload format')
                    source['rect']=((w0>>12)&4095,w0&4095,(w1>>12)&4095,w1&4095)
                else:
                    require((w0&0xffffff)==0,'unsupported block source offset')
                    source['loaded_bytes']=((((w1>>12)&4095)+1)*(4<<tile['size'])+7)//8
                self.tmem[tile['tmem']]=source
            elif op==0xf0:
                require(self.image is not None,'TLUT missing source');tile=self.tiles[(w1>>24)&7]
                require(tile['tmem']==256,'unsupported palette bank')
                self.tlut=dict(address=self.image['address'],count=((w1>>14)&1023)+1)
            elif op==0xfc:self.combine=[w0&0xffffff,w1]
            elif op==0xfa:self.prim_color=list(w1.to_bytes(4,'big'))
            elif op==0xfb:self.env_color=list(w1.to_bytes(4,'big'))
            elif op==0xbe:self.warnings.add('View-dependent G_CULLDL rejection omitted; all original triangles retained')
            elif op in (0,0xe6,0xe7,0xe8,0xe9):pass
            else:raise DecodeError(f'unsupported F3DEX opcode {op:02x} at {pc:08x}')


def decode_model(model, texture_model=None, source=None):
    try:
        from .animation import AnimationRig
    except ImportError:
        from animation import AnimationRig
    es=entries(model)
    direct=not any(e['type']==1 for e in es)
    if direct:
        # Source 8022D58C draws entry0 directly without a hierarchy. A single
        # identity matrix slot expresses that draw contract, not an invented
        # skeleton, pose or animation.
        require([e['type'] for e in es]==[0,7], 'unsupported direct-draw model table')
        try:
            from .pose import Hierarchy, Limb, Transform
        except ImportError:
            from pose import Hierarchy, Limb, Transform
        identity=Transform((0.,0.,0.),(0.,0.,0.),(1.,1.,1.))
        rig=AnimationRig(Hierarchy(0,(Limb(0,None,None,None,0,identity),),None),(),())
    else:
        rig=AnimationRig.parse(model)
    decoder=Decoder(Memory(model,texture_model))
    roots=[]
    for limb in rig.hierarchy.limbs:
        if limb.display_entry == -1:
            continue
        e=es[limb.display_entry]
        require(e['type'] in (0,5),'unsupported drawable hierarchy entry type')
        scale=struct.unpack('>f',struct.pack('>I',e['param']))[0]
        require(math.isfinite(scale) and scale==1.0,'nonunit per-display scale needs source verification')
        label=f'entry_{e["index"]}_joint_{limb.index}'
        roots.append(dict(entry_index=e['index'],joint_index=limb.index,entry_type=e['type'],
                          display_list=0x02000000+e['offset'],billboard=e['type']==5,label=label))
        decoder.run(0x02000000+e['offset'],limb.index,e['type']==5,label)
    require(decoder.triangles,'original model produced no triangles')
    animations=[]
    total_frames=sum(a.duration+1 for a in rig.animations)
    require(total_frames<=10000 and total_frames*len(rig.hierarchy.limbs)<=500000,
            'sampled animation budget exceeded')
    for i,a in enumerate(rig.animations):
        animations.append(dict(index=i,id=a.identifier,duration=a.duration,
                          frames=[[list(t.values()) for t in rig.sample(i,frame)]
                                  for frame in range(a.duration+1)]))
    result=dict(schema_version=SCHEMA,source=source or {},skeleton=rig.hierarchy.to_dict(),roots=roots,
                vertices=decoder.vertices,triangles=decoder.triangles,materials=decoder.materials,
                animations=animations,stats=dict(vertices=len(decoder.vertices),triangles=len(decoder.triangles),
                    materials=len(decoder.materials),textures=len(decoder.texture_payloads),
                    commands=decoder.commands,opcodes=decoder.opcodes,joints=len(rig.hierarchy.limbs),
                    animations=len(rig.animations),frames=total_frames),warnings=sorted(decoder.warnings),
                pose_contract=dict(rotation_units='degrees',matrix_layout='row_major_column_vectors',
                    local_matrix_order='T*Rz*Rx*Ry',limb_scale_rendered=False,
                    source_degree_to_radian=0.01745329238474369,
                    billboard='camera_facing_rotation_cancel_preserve_center_and_object_scale',
                    uv_format='signed16_s10.5_texels',frames='original_integer_animation_units',
                    interpolation='use animations.json original keypose/weight tracks for fractional frames',
                    caller_render_state='opaque original model; scene lights/fog not emulated'))
    rig_json=rig.to_dict()
    if direct:
        for skeleton in (result['skeleton'],rig_json['skeleton']):
            skeleton['source_hierarchy']=False
            skeleton['render_only_identity']=True
    return result,decoder.texture_payloads,rig_json


def convert(rom_path,output):
    import stat
    rom,identity=validate(read_rom(Path(rom_path)))
    output=Path(output).absolute()
    require('..' not in output.parts and not output.exists() and not output.is_symlink(),
            'output must be a new private directory without parent traversal')
    for ancestor in output.parents:
        info=ancestor.lstat()
        require(not stat.S_ISLNK(info.st_mode) and not (getattr(info,'st_file_attributes',0)&0x400),
                'output ancestors cannot be symlinks or reparse points')
    require(output.parent.is_dir(),'output parent must exist')
    selected={i:extract_model(rom,i) for i in (0,1,2,3,4,5,6,16,17,0x49,0x4a)}
    decoded=[]
    for model_id in (0x49,0,1,2,3,4,5,6,16,17):
        data,origin=selected[model_id]
        source=dict(rom=identity,model=origin,texture_model=selected[0x4a][1] if model_id==0x49 else None)
        result,textures,rig=decode_model(data,selected[0x4a][0] if model_id==0x49 else None,source)
        name=('player' if model_id==0x49 else
              f'bomb_{model_id:02x}' if model_id<=5 else f'effect_{model_id:02d}')
        decoded.append((name,result,textures,rig))
    output.mkdir(mode=0o700)
    (output/'.gitignore').write_bytes(b'*\n')
    manifest=dict(schema_version='bm64-original-assets-v1',identity=identity,models=[])
    for name,result,textures,rig in decoded:
        directory=output/name;directory.mkdir(mode=0o700);(directory/'textures').mkdir(mode=0o700)
        for filename,payload in textures.items():
            (directory/'textures'/filename).write_bytes(payload)
        (directory/'animations.json').write_bytes((json.dumps(rig,separators=(',',':'))+'\n').encode('utf-8'))
        encoded=json.dumps(result,separators=(',',':'))+'\n'
        require(len(encoded)<64*1024*1024,'mesh descriptor output limit')
        (directory/'mesh.json').write_bytes(encoded.encode('utf-8'))
        manifest['models'].append(dict(name=name,mesh=name+'/mesh.json',stats=result['stats'],
                                      sha256=hashlib.sha256(encoded.encode()).hexdigest()))
    (output/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode('utf-8'))
    return manifest


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(argv)
    try:
        result=convert(args.rom,args.output)
    except (OSError,ValueError,KeyError,TypeError,RecursionError,struct.error) as error:
        print(f'Original BM64 conversion stopped: {error}',file=sys.stderr)
        return 2
    print(json.dumps(dict(output=str(args.output),models=result['models']),indent=2))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
