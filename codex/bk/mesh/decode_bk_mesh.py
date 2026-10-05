#!/usr/bin/env python3
"""Convert authenticated original Banjo-Kazooie combined duo model and animations.

The source geometry graph, vertex-cache skinning, selectors and original texture
bytes are decoded, never replaced by custom character art or generated poses.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import stat
import struct
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'assets'))
from prepare_bk import read_rom, validate, extract_asset, MODEL_ID, InputError
SCHEMA='bk-original-mesh-v1'
MAX_MODEL=2*1024*1024
MAX_COMMANDS=100000
MAX_VERTICES=20000
MAX_TRIANGLES=20000
MAX_TEXTURE_PIXELS=1024*1024
class DecodeError(ValueError):
    pass

def require(condition,message):
    if not condition: raise DecodeError(message)

def read(data,offset,size,alignment=1):
    require(type(offset) is int and type(size) is int and offset>=0 and size>=0,
            'invalid source range')
    require(offset%alignment==0 and offset<=len(data) and size<=len(data)-offset,
            f'misaligned or out-of-bounds source read {offset:x}+{size:x}')
    return data[offset:offset+size]

def tile_state():
    return dict(fmt=0,size=2,line=0,tmem=0,palette=0,cm_s=0,cm_t=0,
                mask_s=0,mask_t=0,shift_s=0,shift_t=0,uls=0,ult=0,lrs=0,lrt=0)


def rgba16(v):
    rgb = [(v >> s) & 31 for s in (11, 6, 1)]
    return [(x << 3) | (x >> 2) for x in rgb] + [255 if v & 1 else 0]


class Decoder:
    """F3DEX1 source interpreter; unsupported commands fail closed."""
    def __init__(self, memory):
        self.memory = memory
        self.cache = [None]*32
        self.vertices=[]; self.triangles=[]; self.materials=[]
        self.vertex_ids={};self.material_ids={};self.texture_payloads={}
        self.commands=0;self.opcodes={};self.warnings=set()
        # baModel_draw sets full depth; each source material replaces shade,
        # culling and lighting bits. modelRender dynamic-env setup uses black
        # primitive color and scene tint; the host profile uses neutral white tint.
        self.geometry=1 # G_ZBUFFER
        self.combine=[0,0];self.prim_color=[0]*4;self.env_color=[255]*4
        self.texture_enabled=False;self.texture_scale=[65535]*2;self.texture_tile=0
        self.tiles=[tile_state() for _ in range(8)]
        # modelRender setup2CycleBlackPrimDL: two-cycle, one-primitive pipeline.
        # Remaining sampler defaults are the nonfog host profile; source commands
        # overwrite TLUT and render-mode bits before textured primitives.
        self.image=None;self.tluts={};self.tmem={};self.othermode_h=0x982c30;self.othermode_l=0
        self.light_diffuse=[255]*3;self.light_ambient=[127]*3
        self.joint=-1;self.billboard=False;self.root=None
        self.matrix_stack=[-1];self.selectors=[]

    def texture(self, tile):
        if not self.texture_enabled:
            return None
        # SHADE-only combine does not sample TMEM despite stale texture enable.
        if self.combine == [0xffffff,0xfffe7d3e]:
            return None
        source=self.tmem.get(tile['tmem'])
        require(source is not None,'texture primitive reads unloaded TMEM')
        require(tile['fmt']==2 and tile['size']==0,'profile only supports verified original CI4 materials')
        tlut=self.tluts.get(tile['palette'])
        require(tlut is not None and ((self.othermode_h>>14)&3)==2,'CI texture requires loaded RGBA16 palette')
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
        paladdr=tlut['address']
        available=self.memory.palette_catalog.get(paladdr,tlut['count'])
        require(all(index < available for index in raw),'CI index exceeds actual original palette')
        # Bound CI indices to the catalog palette; never read another texture
        # or unrelated model bytes when reconstructing original RGBA colors.
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
        self.triangles.append(dict(indices=[self.vertex(v) for v in vs],material=self.material(),root=self.root,selectors=copy.deepcopy(self.selectors)))

    def run(self,address,joint,billboard,label):
        self.joint,self.billboard,self.root=joint,billboard,label
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
            elif op==0xbd:
                require(w1==0 and len(self.matrix_stack)>1,'invalid source matrix pop')
                self.matrix_stack.pop();self.joint=self.matrix_stack[-1]
            elif op==6:
                mode=(w0>>16)&255;require(mode in (0,1),'invalid list branch')
                if w1>>24==3:
                    require(w1 in (0x03000010,0x03000030),'unsupported original render-mode table entry')
                    self.othermode_l=0x0c192078 if w1==0x03000010 else 0x0c1841f8
                else:
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
                require(256<=tile['tmem']<512 and tile['tmem']%16==0,'invalid palette bank')
                self.tluts[(tile['tmem']-256)//16]=dict(address=self.image['address'],count=((w1>>14)&1023)+1)
            elif op==0xfc:self.combine=[w0&0xffffff,w1]
            elif op==0xfa:self.prim_color=list(w1.to_bytes(4,'big'))
            elif op==0xfb:self.env_color=list(w1.to_bytes(4,'big'))
            elif op==0xbe:self.warnings.add('View-dependent G_CULLDL rejection omitted; all original triangles retained')
            elif op in (0,0xe6,0xe7,0xe8,0xe9):pass
            else:raise DecodeError(f'unsupported F3DEX opcode {op:02x} at {pc:08x}')


class Memory:
    """Original model segments: 1=Vtx array,2=texture payload,4=local Gfx."""
    def __init__(self,model):
        require(isinstance(model,bytes) and 56<=len(model)<=MAX_MODEL,'invalid model length')
        require(struct.unpack_from('>I',model)[0]==11,'unsupported BK model magic')
        self.model=model
        self.geo=struct.unpack_from('>I',model,4)[0]
        tex=struct.unpack_from('>H',model,8)[0]
        require(struct.unpack_from('>H',model,10)[0]==0,'unsupported original player geo flags')
        gfx,vtx=struct.unpack_from('>II',model,12)
        anim=struct.unpack_from('>I',model,24)[0]
        require(56==tex<gfx<vtx<anim<self.geo<len(model),'invalid model section order')
        self.bone_count=struct.unpack('>h',read(model,anim+4,2))[0]
        require(1<=self.bone_count<=109,'original model bone count limit')
        self.gfx_count,pad=struct.unpack('>II',read(model,gfx,8,8))
        require(pad==0 and 0<self.gfx_count<20000 and gfx+8+self.gfx_count*8==vtx,
                'invalid display-list extent')
        vertex_count=struct.unpack('>H',read(model,vtx+20,2))[0]
        require(vertex_count==struct.unpack_from('>H',model,50)[0] and
                vtx+24+vertex_count*16<=anim,'invalid vertex count')
        texture_size,texture_count,pad=struct.unpack('>IHH',read(model,tex,8,8))
        require(0<texture_count<=256 and pad==0 and tex+texture_size==gfx,
                'invalid texture list')
        payload_start=tex+8+texture_count*16
        payload=read(model,payload_start,gfx-payload_start)
        self.segments={1:read(model,vtx+24,vertex_count*16,8),2:payload,
                       4:read(model,gfx+8,self.gfx_count*8,8)}
        self.texture_catalog={};self.palette_catalog={};self.textures=[]
        prior_end=0
        for index in range(texture_count):
            off,kind,w,h=struct.unpack('>Ih2xBB6x',read(model,tex+8+index*16,16,8))
            require(kind==1 and 0<w<=128 and 0<h<=128,'only verified original CI4 textures accepted')
            n=32+(w*h+1)//2
            require(off==prior_end and off+n<=len(payload),'invalid original texture extent')
            self.palette_catalog[0x02000000+off]=16
            self.texture_catalog[0x02000000+off+32]=(w,h)
            self.textures.append(dict(index=index,width=w,height=h,type=kind,offset=off,size=n))
            prior_end=off+n
        require(prior_end==len(payload),'unaccounted original texture data')
    def read(self,address,count,alignment=1):
        require(type(address) is int and 0<=address<=0xffffffff,'invalid segmented address')
        segment=address>>24
        require(segment in self.segments,f'unsupported source segment {segment}')
        return read(self.segments[segment],address&0xffffff,count,alignment)

class Geometry:
    """Source graph interpreter, retaining branch conditions and Vtx-load matrices.

    Highest-detail LOD only. This is the original nearest model branch, not a
    synthesized mesh. Alternative selectors are isolated at entry; tests compare
    filtered output with independent source execution for concrete selections.
    """
    def __init__(self,memory,decoder,selector_values=None):
        self.memory=memory;self.decoder=decoder;self.count=0
        self.selector_values=selector_values
        self.selector_counts={};self.lod_ranges=[]
    def dl(self,index):
        require(0<=index<self.memory.gfx_count,'display index outside original list')
        self.decoder.root=f'gfx_{index:04x}'
        self.decoder._list(0x04000000+index*8,[])
    def state(self):
        keys=('cache','geometry','combine','prim_color','env_color','texture_enabled',
              'texture_scale','texture_tile','tiles','image','tluts','tmem','othermode_h',
              'othermode_l','light_diffuse','light_ambient','joint','matrix_stack','root')
        return {k:copy.deepcopy(getattr(self.decoder,k)) for k in keys}
    def restore(self,state):
        for k,v in state.items():setattr(self.decoder,k,copy.deepcopy(v))
    def walk(self,start,active=()):
        require(len(active)<64 and start not in active,'geometry recursion/cycle')
        active=active+(start,);p=start;visited=set();d=self.decoder;m=self.memory.model
        while True:
            self.count+=1
            require(self.count<=10000 and p not in visited,'geometry command budget/cycle')
            visited.add(p)
            op,n=struct.unpack('>II',read(m,p,8,4))
            require(n==0 or n>=8 and n%4==0 and p+n<len(m),'invalid geometry next offset')
            if op==2:
                branch,joint=struct.unpack('>Bb',read(m,p+8,2))
                require(-1<=joint<self.memory.bone_count,'invalid geometry matrix ID')
                prior=d.matrix_stack[:]
                d.matrix_stack.append(joint);d.joint=joint
                if branch:self.walk(p+branch,active)
                require(d.matrix_stack==prior+[joint],'unbalanced original bone matrix stack')
                d.matrix_stack.pop();d.joint=d.matrix_stack[-1]
            elif op==3:
                self.dl(struct.unpack('>h',read(m,p+8,2))[0])
            elif op==5:
                joint=d.joint;initial=d.matrix_stack[:]
                self.dl(struct.unpack('>h',read(m,p+8,2))[0])
                q=p+10
                while True:
                    index=struct.unpack('>h',read(m,q,2))[0];q+=2
                    if index==0:break
                    require(q<=p+64,'skinning display-list limit')
                    d.matrix_stack.append(joint);d.joint=joint
                    self.dl(index)
                require(d.matrix_stack==initial,'unbalanced original skinning matrix stack')
            elif op==8:
                far,near=struct.unpack('>ff',read(m,p+8,8))
                branch=struct.unpack('>I',read(m,p+28,4))[0]
                require(math.isfinite(far) and math.isfinite(near) and 0<=near<far,
                        'invalid original LOD range')
                self.lod_ranges.append([near,far])
                if near==0 and branch:self.walk(p+branch,active)
            elif op==10:
                read(m,p+8,16) # original reference point: no drawable triangles
            elif op==12:
                count,index=struct.unpack('>hh',read(m,p+8,4))
                require(0<count<=64 and 1<=index<64,'invalid geometry selector')
                branches=struct.unpack('>'+str(count)+'I',read(m,p+12,4*count))
                require(index not in self.selector_counts or self.selector_counts[index]==count,
                        'inconsistent selector branch count')
                self.selector_counts[index]=count
                if self.selector_values is not None:
                    selection=self.selector_values.get(index,0)
                    if selection>0:
                        require(selection<=count,'invalid concrete selector')
                        self.walk(p+branches[selection-1],active)
                else:
                    saved=self.state();post_first=None
                    for i,branch in enumerate(branches):
                        self.restore(saved);d.selectors.append([index,i+1])
                        self.walk(p+branch,active);d.selectors.pop()
                        if i==0:post_first=self.state()
                    self.restore(post_first)
            else:
                raise DecodeError(f'unsupported geometry opcode {op} at {p:x}')
            if n==0:return
            p+=n

def decode_model(model,source=None,selector_values=None):
    try:
        from .animation import parse_model_skeleton
    except ImportError:
        from animation import parse_model_skeleton
    memory=Memory(model);decoder=Decoder(memory);geo=Geometry(memory,decoder,selector_values)
    geo.walk(memory.geo)
    require(decoder.matrix_stack==[-1],'model leaves matrix stack unbalanced')
    require(decoder.triangles,'original model produced no triangles')
    skeleton=parse_model_skeleton(model)
    return dict(schema_version=SCHEMA,source=source or {},skeleton=skeleton,
                vertices=decoder.vertices,triangles=decoder.triangles,materials=decoder.materials,
                selectors=[dict(index=i,branches=n) for i,n in sorted(geo.selector_counts.items())],
                original_textures=memory.textures,stats=dict(vertices=len(decoder.vertices),
                    triangles=len(decoder.triangles),materials=len(decoder.materials),
                    textures=len(decoder.texture_payloads),joints=memory.bone_count,
                    commands=decoder.commands,geometry_commands=geo.count,opcodes=decoder.opcodes),
                pose_contract=dict(positions='original_model_space',
                    skinning='original_vertex_cache_matrix_at_G_VTX',identity_joint=-1,
                    matrix_palette='original_model_matrix_indices_plus_final_actor_identity',
                    texture_uv='signed_s10.5_scaled_at_vertex_load',
                    geometry_lod='original_nearest_detail_branch',lod_ranges=geo.lod_ranges,
                    selector_semantics='all [index,one_based_branch] conditions must match'),
                warnings=['High-detail original LOD is retained at every host camera distance',
                          'Host material renderer approximates N64 coverage, filtering and scene fog']),decoder.texture_payloads

ANIMATIONS=(
    (0x6f,'idle'),(0x2,'creep'),(0x3,'walk'),(0xc,'run'),(0x8,'jump'),(0xb0,'fall'),
    (0x1,'crouch_enter'),(0x10c,'crouch_idle'),(0x16,'trot_enter'),(0x26,'trot_idle'),
    (0x15,'trot'),(0x27,'trot_jump'),(0x7,'trot_exit'),(0x4b,'flip_enter'),
    (0x4c,'flip_hold'),(0x61,'flip_exit'),(0x18,'flap_enter'),(0x17,'flap'),
    (0x5,'claw'),(0x4f,'roll'),(0x1c,'barge'),(0x1a,'peck_enter_exit'),
    (0x19,'peck_loop'),(0x1d,'beak_buster'))

def convert(rom_path,output):
    try:
        from .animation import decode_animation
    except ImportError:
        from animation import decode_animation
    rom,identity=validate(read_rom(rom_path))
    output=Path(output).absolute()
    require('..' not in output.parts and not output.exists() and not output.is_symlink(),
            'output must be a new private directory without traversal')
    for ancestor in output.parents:
        info=ancestor.lstat()
        require(not stat.S_ISLNK(info.st_mode) and not getattr(info,'st_file_attributes',0)&0x400,
                'output ancestors cannot be symlinks or reparse points')
    require(output.parent.is_dir(),'output parent must exist')
    model,origin=extract_asset(rom,MODEL_ID)
    mesh,textures=decode_model(model,dict(rom=identity,model=origin))
    animations=[]
    for asset_id,name in ANIMATIONS:
        data,info=extract_asset(rom,asset_id)
        animation=decode_animation(data,asset_id,name);animation['source']=info
        animations.append(animation)
    rig=dict(format='bk-original-animation-v1',source=mesh['source'],skeleton=mesh['skeleton'],animations=animations)
    files={'player/mesh.json':(json.dumps(mesh,separators=(',',':'))+'\n').encode(),
           'player/animations.json':(json.dumps(rig,separators=(',',':'))+'\n').encode()}
    files.update({'player/textures/'+name:payload for name,payload in textures.items()})
    require(sum(map(len,files.values()))<32*1024*1024,'generated asset budget exceeded')
    manifest=dict(schema_version='bk-original-assets-v1',identity=identity,
                  models=[dict(name='player',mesh='player/mesh.json',animations='player/animations.json',stats=mesh['stats'])],
                  files=[dict(path=name,size=len(payload),sha256=hashlib.sha256(payload).hexdigest())
                         for name,payload in sorted(files.items())])
    output.mkdir(mode=0o700)
    (output/'.gitignore').write_bytes(b'*\n')
    (output/'player').mkdir(mode=0o700);(output/'player'/'textures').mkdir(mode=0o700)
    for name,payload in files.items():
        with (output/name).open('xb') as stream:stream.write(payload)
    with (output/'manifest.json').open('x',encoding='utf8') as stream:
        json.dump(manifest,stream,indent=2);stream.write('\n')
    return manifest

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('rom',type=Path);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args(argv)
    try:result=convert(args.rom,args.output)
    except (OSError,ValueError,KeyError,TypeError,RecursionError,struct.error) as error:
        print(f'Original BK conversion stopped: {error}',file=sys.stderr);return 2
    print(json.dumps(dict(output=str(args.output),models=result['models']),indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
