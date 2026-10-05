#!/usr/bin/env python3
"""Decode USA 1.0 authored combat data from the user's verified private boot.

No ROM, code bytes or authored tables are distributed with this module. The output
is a private runtime asset. This reproduces the table walks at 0x800A15AC; it does
not execute or copy original implementation code.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT))
from codex.spiderman.movement.original_mips import BOOT_SHA256, ROM_SHA256
MAGIC=b'SMN64C02'
BASE=0x80016ae0

def decode(boot):
    if len(boot)!=995056 or hashlib.sha256(boot).hexdigest()!=BOOT_SHA256:
        raise ValueError('Unsupported private USA 1.0 boot image')
    def val(a,t='H'):
        o=a-BASE;s=struct.calcsize('>'+t)
        if o<0 or o+s>len(boot):raise ValueError('Combat table out of bounds')
        return struct.unpack_from('>'+t,boot,o)[0]
    def stream(a,limit):
        out=[]
        while val(a,'B')!=255:
            out.append(val(a,'B'));a+=1
            if len(out)>limit:raise ValueError('Combat stream over bound')
        return out,a+1
    roots={};p=0x800fa5e0
    while val(p)!=0x1a0a:
        slot=val(p);p+=2
        if not 0<=slot<300 or slot in roots:raise ValueError('Bad root animation slot')
        data,p=stream(p,256);roots[slot]=dict(address=p-len(data)-1,values=data)
        p=(p+3)&~3
    combos={};p=0x800fa6e8
    while val(p,'I')!=0x1a0a:
        slot=val(p);n=val(p+0x20)
        if not 0<=slot<32 or slot in combos or n>16:raise ValueError('Bad combo record')
        c=dict(address=p,id=slot,animation=val(p+2),damage=val(p+8),
          hit_start=val(p+12),hit_end=val(p+14),normal_start=val(p+18),normal_end=val(p+20),
          alternate_start=val(p+22),alternate_end=val(p+24),impulse=val(p+26),duration=val(p+28),
          hit_flags=val(p+16,'B'),alternate=val(p+34),branches=[])
        for i in range(n):
            q=p+36+i*12
            c['branches'].append(dict(target=val(q,'I'),at=val(q+4),offset=val(q+6),
               animation=val(q+8),flag=val(q+10)))
        q=p+36+n*12
        c['sequence'],q=stream(q,128);c['bones'],q=stream(q,32);c['frames'],q=stream(q,256)
        if not c['frames'] or any(x>3 for x in c['sequence'][:1]+c['sequence'][1::2]) or c['animation']>=300:
            raise ValueError('Invalid combo streams')
        combos[slot]=c;p=(q+3)&~3
    for c in combos.values():
        for b in c['branches']:
            if b['target'] not in combos:raise ValueError('Unresolved combo successor')
    return {'source':{'rom_sha256':ROM_SHA256,'boot_sha256':BOOT_SHA256},'combos':combos,'roots':roots,'air_types':[ {0x80092f5c:0,0x80092e94:1,0x80092f3c:2}[val(0x80020e78+4*i,'I')] for i in range(21)]}

def encode(data):
    out=bytearray(MAGIC+bytes.fromhex(ROM_SHA256)+bytes.fromhex(BOOT_SHA256))
    for slot in range(32):
        c=data['combos'].get(slot)
        fields=['id','animation','damage','hit_start','hit_end','normal_start','normal_end',
            'alternate_start','alternate_end','impulse','duration','hit_flags','alternate']
        row=[1]+[c[k] for k in fields]+[len(c[k]) for k in ('branches','sequence','bones','frames')] if c else [0]*18
        out.extend(struct.pack('<18H',*row))
        for i in range(16):
            b=c['branches'][i] if c and i<len(c['branches']) else None
            out.extend(struct.pack('<5H',*[b[k] for k in ('target','at','offset','animation','flag')]) if b else bytes(10))
        for name,n in [('sequence',128),('bones',32),('frames',256)]:
            v=bytes(c[name]) if c else b'';out.extend(v+bytes(n-len(v)))
    for slot in range(300):
        v=bytes(data['roots'].get(slot,{}).get('values',[]))
        out.extend(struct.pack('<H',len(v)));out.extend(v+bytes(256-len(v)))
    out.extend(bytes(data['air_types']))
    return bytes(out)

def main():
    p=argparse.ArgumentParser();p.add_argument('--boot',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();data=decode(a.boot.read_bytes());a.out.parent.mkdir(parents=True,exist_ok=True)
    blob=encode(data);a.out.write_bytes(blob)
    print(json.dumps({'output':str(a.out),'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest(),
      'combo_records':len(data['combos']),'root_streams':len(data['roots']),**data['source']}))
if __name__=='__main__':main()
