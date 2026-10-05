"""Privately export original-MIPS Q12 pitch/yaw matrices; no asset data bundled.
Usage: python air_spin_export_rotations.py [original.z64] [boot.bin] [output.bin]
Format: <8sII header b'T1ARQ12\\0',4096,2, then pitch4096x9 signed LE16,
then yaw4096x9 signed LE16. Index=source angle&4095. Total147472 bytes.
"""
from pathlib import Path
import hashlib
import struct
import sys
from unicorn import Uc,UC_ARCH_MIPS,UC_MODE_MIPS32,UC_MODE_BIG_ENDIAN
from unicorn.mips_const import UC_MIPS_REG_A0,UC_MIPS_REG_A1,UC_MIPS_REG_RA,UC_MIPS_REG_SP,UC_MIPS_REG_PC
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT))
from codex.thps.assets.archive import load_rom
def export(rompath,bootpath,out):
 rom=load_rom(rompath);boot=Path(bootpath).read_bytes()
 assert hashlib.sha256(rom).hexdigest()=='506961e65197aaff2b47ae055b7d28470f80fb925914ebeac9ddc541a7a9fd8d'
 assert hashlib.sha256(boot).hexdigest()=='e9e91933301ad4f74750ca5c9082490af1cb27a5d2c76371fe1e100721a84395'
 u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_BIG_ENDIAN);u.mem_map(0,0x800000)
 u.mem_write(0x400,rom[0x1000:0x1000+0x166c0]);u.mem_write(0x16ac0,boot)
 data=bytearray(struct.pack('<8sII',b'T1ARQ12\0',4096,2))
 for axis in (0,1):
  for angle in range(4096):
   angles=[0,0,0];angles[axis]=angle
   u.mem_write(0x200000,struct.pack('>3h',*angles))
   for reg,value in [(UC_MIPS_REG_A0,0x80200000),(UC_MIPS_REG_A1,0x80200100),(UC_MIPS_REG_RA,0x80700000),(UC_MIPS_REG_SP,0x80600000)]:u.reg_write(reg,value)
   u.emu_start(0x80022598,0x80700000,count=5000)
   assert u.reg_read(UC_MIPS_REG_PC)==0x80700000
   data.extend(struct.pack('<9h',*struct.unpack('>9h',u.mem_read(0x200100,18))))
 out=Path(out);out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
 return {'file':str(out),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'entries':8192,'source':'original22598 with original ROM sin/cos'}
if __name__=='__main__':
 args=sys.argv[1:]
 defaults=[ROOT/'codex/.runtime/thps/original.z64',ROOT/'codex/.runtime/thps/boot.bin',ROOT/'codex/.runtime/thps/air_spin_rotations.bin']
 print(export(*[args[i] if i<len(args) else defaults[i] for i in range(3)]))
