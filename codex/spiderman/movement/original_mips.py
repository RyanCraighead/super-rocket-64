"""Read-only, exact-input MIPS oracle utilities. No original code bytes bundled."""
from pathlib import Path
import hashlib
import struct

ROM_SHA256 = 'feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c'
BOOT_SHA256 = '1d3ed3384f45ada2ebf6cb0666ddc7fec4c4ffdb6fdb993b8d566aa3cd4f3867'

class OriginalMips:
    """Runs bounded original routines with only explicit input memory.

    Addresses are original virtual KSEG0 addresses. Unicorn memory reads/writes
    use physical addresses. No call hook or substituted math implementation.
    This is a CPU-function oracle, not a full N64 game emulator.
    """
    def __init__(self, boot_path, rom_path=None):
        from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_BIG_ENDIAN
        from unicorn.mips_const import (UC_MIPS_REG_A0, UC_MIPS_REG_A1,
            UC_MIPS_REG_A2, UC_MIPS_REG_A3, UC_MIPS_REG_RA, UC_MIPS_REG_PC, UC_MIPS_REG_SP)
        self.args = [UC_MIPS_REG_A0, UC_MIPS_REG_A1, UC_MIPS_REG_A2, UC_MIPS_REG_A3]
        self.ra,self.pc,self.sp = UC_MIPS_REG_RA,UC_MIPS_REG_PC,UC_MIPS_REG_SP
        from codex.spiderman.inspect_n64_input import read_input
        boot, boot_member, _boot_source_hash = read_input(Path(boot_path))
        if boot_member is not None or len(boot) != 995056:
            raise ValueError("Expected one bounded decoded boot file")
        if hashlib.sha256(boot).hexdigest() != BOOT_SHA256:
            raise ValueError('Unsupported boot image SHA256')
        self.u = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_BIG_ENDIAN)
        self.u.mem_map(0, 0x800000)
        self.has_prefix = rom_path is not None
        if rom_path is not None:
            # Share the hardened regular-file, ZIP, size, and revision gates.
            # The repository root is required on PYTHONPATH for this package.
            from codex.spiderman.assets.archive import load_rom
            rom = load_rom(rom_path)
            # Startup entry is 0x80000400, loaded from ROM0x1000; raw code/data
            # ends at0x80012F30. Startup clears BSS through0x80016AE0.
            self.u.mem_write(0x400, rom[0x1000:0x13b30])
        self.u.mem_write(0x16ae0, boot)

    def call(self, address, *args, instruction_limit=5000):
        for reg,value in zip(self.args,args): self.u.reg_write(reg,value & 0xffffffff)
        self.u.reg_write(self.ra,0x80700000)
        self.u.reg_write(self.sp,0x80600000)
        self.u.emu_start(address,0x80700000,count=instruction_limit)
        if self.u.reg_read(self.pc) != 0x80700000:
            raise RuntimeError('Original routine exceeded instruction budget')

    def rotation_fixed12(self, angles):
        if not self.has_prefix:
            raise ValueError('Rotation requires verified raw application prefix')
        self.u.mem_write(0x200000,struct.pack('>hhh',*angles))
        self.u.mem_write(0x300000,b'\xa5'*0x40)
        self.call(0x80053bfc,0x80200000,0x80300000)
        return struct.unpack('>9h',self.u.mem_read(0x300000,18))

    def compressed_frame_pose(self, frame, parents):
        """Execute the original 0x2C cached-frame hierarchy path.

        `frame` is bone-ordered six-s16 source samples; `parents` is the original
        bone parent table with exactly one self-parent root (or -1 for root).
        This tests post-decode hierarchy, not compressed stream decoding.
        Returns original float32 4x4 matrices plus converted twelve-s16 poses.
        """
        if not self.has_prefix:
            raise ValueError('Pose math requires original raw application prefix')
        n = len(frame)
        if not 1 <= n <= 256 or len(parents) != n or any(len(row) != 6 for row in frame):
            raise ValueError('Invalid frame shape')
        p = [i if parent == -1 else parent for i, parent in enumerate(parents)]
        if any(parent < 0 or parent >= n for parent in p):
            raise ValueError('Invalid parent index')
        roots = [i for i, parent in enumerate(p) if i == parent]
        if len(roots) != 1:
            raise ValueError('Expected exactly one hierarchy root')
        order = roots.copy()
        for parent in order:
            order.extend(i for i, pi in enumerate(p) if pi == parent and i != parent)
            if len(order) > n: raise ValueError('Cyclic hierarchy')
        if len(order) != n or len(set(order)) != n:
            raise ValueError('Disconnected or cyclic hierarchy')
        def w16(addr,value): self.u.mem_write(addr,struct.pack('>H',value))
        def w32(addr,value): self.u.mem_write(addr,struct.pack('>I',value))
        obj, region, mesh, meta, bank = 0x200000,0xf8138,0x210004,0x220000,0x230008
        parents_at, order_at, buffer, floats = 0x240000,0x250000,0x300000,0x400000
        self.u.mem_write(obj,b'\0'*0x200)
        self.u.mem_write(region,b'\0'*64)
        w32(region+0x10,mesh|0x80000000)
        w32(mesh-4,n)
        w32(region+0x14,meta|0x80000000)
        w32(meta+8,n)
        w32(region+0x20,bank|0x80000000)
        w32(bank-8,0x2c)
        w32(region+0x24,parents_at|0x80000000)
        self.u.mem_write(parents_at,struct.pack('>'+str(n)+'H',*p))
        self.u.mem_write(order_at,struct.pack('>'+str(n)+'H',*order))
        w32(obj+0x130,order_at|0x80000000)
        w32(obj+0x134,buffer|0x80000000)
        w32(obj+0x138,floats|0x80000000)
        w16(obj+0x13c,roots[0])
        w16(obj+0x13e,0)  # Cached source clip 0; skip original allocator/decoder.
        w16(obj+0x140,65535)  # Prior frame differs; force hierarchy evaluation.
        self.u.mem_write(buffer,b'\xa5'*(24*n))
        self.u.mem_write(buffer+24*n,struct.pack('>'+str(6*n)+'h',*(x for row in frame for x in row)))
        self.u.mem_write(floats,b'\xa5'*(64*n))
        self.call(0x80071aa4,obj|0x80000000,instruction_limit=1500*n+1000)
        raw_float = bytes(self.u.mem_read(floats,64*n))
        raw_pose = bytes(self.u.mem_read(buffer,24*n))
        return {
            'matrices_f32': [list(struct.unpack_from('>16f',raw_float,i*64)) for i in range(n)],
            'poses_s16': [list(struct.unpack_from('>12h',raw_pose,i*24)) for i in range(n)],
            'matrices_f32be_sha256': hashlib.sha256(raw_float).hexdigest(),
            'poses_s16be_sha256': hashlib.sha256(raw_pose).hexdigest(),
        }
