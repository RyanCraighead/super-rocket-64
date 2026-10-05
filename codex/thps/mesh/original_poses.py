"""Execute USA Rev1's original cached compressed animation pose routine.

No ROM machine code is embedded, hooked, or replaced. Unicorn runs the native
float32 hierarchy and original sine/cosine; owned input files are hash-gated.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from codex.thps.assets.archive import load_rom,require,sha256,read_input

BOOT_SHA256='e9e91933301ad4f74750ca5c9082490af1cb27a5d2c76371fe1e100721a84395'
POLICY={'kind':'original THPS1 N64 MIPS execution, cached decoded 0x2C frame path','entry_address':'0x8007EBE4','angle_matrix_address':'0x80022C04','matrices_f32_layout':'column-major 4x4, raw model vertex units, absolute per-part rotation and world translation','translation_channel_multiplier':0.125,'excluded':'allocator, source clip selection, input, cadence, blending and full game execution'}

class OriginalPose:
    def __init__(self,rom_path,boot_path):
        from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_BIG_ENDIAN
        from unicorn.mips_const import UC_MIPS_REG_A0,UC_MIPS_REG_RA,UC_MIPS_REG_PC,UC_MIPS_REG_SP
        self.a0,self.ra,self.pc,self.sp=UC_MIPS_REG_A0,UC_MIPS_REG_RA,UC_MIPS_REG_PC,UC_MIPS_REG_SP
        rom=load_rom(rom_path);boot,member,_=read_input(Path(boot_path))
        require(member is None,'expected ordinary decoded THPS1 boot')
        require(len(boot)==915872 and sha256(boot)==BOOT_SHA256,'THPS1 boot identity mismatch')
        self.u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_BIG_ENDIAN);self.u.mem_map(0,0x800000)
        self.u.mem_write(0x400,rom[0x1000:0x13b10]);self.u.mem_write(0x16ac0,boot)
    def frame(self,raw,parents):
        n=len(raw);require(n==19 and len(parents)==n,'expected exact19 joints')
        p=[i if v<0 else v for i,v in enumerate(parents)];roots=[i for i,v in enumerate(p) if v==i];require(len(roots)==1,'single root required')
        order=roots.copy()
        for v in order:
            order.extend(i for i,x in enumerate(p) if x==v and i!=v)
            require(len(order)<=n,'cyclic hierarchy')
        require(len(set(order))==n,'disconnected hierarchy')
        u=self.u
        def w32(a,v):u.mem_write(a,struct.pack('>I',v))
        def w16(a,v):u.mem_write(a,struct.pack('>H',v))
        obj,region,mesh,meta,bank=0x200000,0xdaf98,0x210004,0x220000,0x230008
        parents_at,order_at,floats=0x240000,0x250000,0x300000
        u.mem_write(obj,b'\0'*0x300);u.mem_write(region,b'\0'*72)
        w32(region+0x18,mesh|0x80000000);w32(mesh-4,n)
        w32(region+0x1c,meta|0x80000000);w32(meta+8,n)
        w32(region+0x20,bank|0x80000000);w32(bank-8,0x2c);w32(bank+8,1)
        w32(region+0x24,parents_at|0x80000000)
        u.mem_write(parents_at,struct.pack('>19H',*p));u.mem_write(order_at,struct.pack('>19H',*order))
        w32(obj+0x20c,order_at|0x80000000);w32(obj+0x214,floats|0x80000000)
        w16(obj+0x218,roots[0]);u.mem_write(obj+0x21a,b'\0\xff')
        u.mem_write(floats,b'\xa5'*(64*n));u.mem_write(floats+64*n,struct.pack('>114h',*(v for row in raw for v in row)))
        u.reg_write(self.a0,obj|0x80000000);u.reg_write(self.ra,0x80700000);u.reg_write(self.sp,0x80600000)
        u.emu_start(0x8007ebe4,0x80700000,count=1500*n+1000)
        require(u.reg_read(self.pc)==0x80700000,'THPS1 pose instruction budget exceeded')
        output=bytes(u.mem_read(floats,64*n))
        return {'matrices_f32':[list(struct.unpack_from('>16f',output,64*i)) for i in range(n)],'matrices_f32be_sha256':sha256(output)}

def run(rom,directory):
    directory=Path(directory);path=directory/'thps_model.json';model=json.loads(path.read_text());oracle=OriginalPose(rom,directory/'boot.bin');parents=[o['parent'] for o in model['objects']];count=0
    for ref in model['animations']:
        p=directory/ref['file'];clip=json.loads(p.read_text());require(clip['status']=='decoded','failed clip')
        clip['original_poses']=[oracle.frame(f,parents) for f in clip['frames']]
        clip['original_pose_execution']=POLICY;ref['original_pose_execution']=True;p.write_text(json.dumps(clip,separators=(',',':'))+'\n');count+=len(clip['frames'])
    model['original_pose_execution']=POLICY
    model['coordinate_contract']['composed_matrix_policy']=POLICY['kind']
    model['coordinate_contract']['native_matrix_translation_multiplier']=8
    model['limitations']=[s for s in model['limitations'] if not s.startswith('Pose matrices are')]
    path.write_text(json.dumps(model,separators=(',',':'))+'\n')
    boot=(directory/'boot.bin').read_bytes();evidence={'policy':POLICY,'frames':count,'bone_poses':count*19,'boot_sha256':BOOT_SHA256,'routines':{f'{a:#x}-{b:#x}':sha256(boot[a-0x80016ac0:b-0x80016ac0]) for a,b in ((0x8007ebe4,0x8007f168),(0x80022c04,0x80022da4),(0x8007e7a0,0x8007ebe4))}}
    from codex.thps.mechanics.air_spin_export_rotations import export
    evidence['body_rotation_table']=export(rom,directory/'boot.bin',directory/'air_spin_rotations.bin')
    (directory/'original-pose-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(evidence,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom');p.add_argument('--directory',default='codex/.runtime/thps-assets');a=p.parse_args();run(a.rom,a.directory)
