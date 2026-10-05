"""Bounded original Spider-Man (USA) N64 asset archive reader.

Format algorithms adapted from slfx77/neversoft-multitool, MIT, pinned
3c9028e0178deb9060190362d1c47039a32846e2. See ../mesh/SOURCES.md.
No game data is distributed by this module.
"""
from __future__ import annotations
import hashlib
import struct
from pathlib import Path
from codex.spiderman.inspect_n64_input import InputError, read_input

ROM_SHA256 = 'feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c'
ROM_SIZE = 33554432
MAX_OUTPUT = 1 << 24

class FormatError(ValueError):
    pass

def require(ok, message):
    if not ok:
        raise FormatError(message)

def u32(data, p=0):
    require(0 <= p <= len(data)-4, 'u32 out of bounds')
    return struct.unpack_from('>I', data, p)[0]

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def load_rom(path):
    # Reuse the hardened regular-file/size/ZIP gate. Probing a path with
    # is_zipfile before validating it can block forever on an untrusted FIFO.
    try:
        data, _member, _source_hash = read_input(Path(path))
    except (InputError, OSError) as error:
        raise FormatError('ROM input rejected: ' + str(error)) from error
    require(len(data) == ROM_SIZE and sha256(data) == ROM_SHA256, 'ROM identity mismatch; no extraction performed')
    require(data[:4] == b'\x80\x37\x12\x40', 'expected big-endian z64')
    return data

def decode_erz(block):
    """ERZ v2, with byte reads bounded to compressedSize, exact output length."""
    require(len(block) >= 18 and block[:4] == b'ERZ\x02', 'expected ERZ version 2')
    size, compressed = u32(block,4), u32(block,8)
    require(size <= MAX_OUTPUT and 0 < compressed <= len(block)-18, 'invalid ERZ sizes')
    end, i = 18+compressed, 18
    out = bytearray()
    def next_byte():
        nonlocal i
        require(i < end, 'ERZ stream overrun')
        v=block[i]; i+=1
        return v
    def put(v):
        require(len(out) < size, 'ERZ output overrun')
        out.append(v)
    bits = (next_byte()*2+1)*2
    def bit():
        nonlocal bits
        bits *= 2
        b=(bits>>8)&1
        bits &= 255
        if bits == 0:
            bits=next_byte()*2+b
            b=(bits>>8)&1
        return b
    operations = 0
    while True:
        operations += 1
        require(operations <= max(1024, size*3+1024), 'ERZ operation limit')
        if bit() == 0:
            put(next_byte())
            continue
        length, high, distance_stage = 2, 0, True
        if bit() == 0:
            length=length*2+bit()
            if bit():
                length=(length-1)*2+bit()
                if length == 9:
                    for _ in range(4):
                        high=high*2+bit()
                    for _ in range((high+3)*4):
                        put(next_byte())
                    continue
        elif bit() == 0:
            distance_stage=False
        elif bit() == 0:
            length=3
        else:
            length=next_byte()
            if length == 0:
                if bit():
                    continue
                require(len(out) == size, 'ERZ premature end')
                return bytes(out)
            length+=8
        if distance_stage and bit():
            high=bit()
            if bit():
                high=(high*2+bit())|4
                if not bit():
                    high=high*2+bit()
            elif high == 0:
                high=2+bit()
            high=(high>>8)|((high<<8)&0xff00)
        distance=next_byte()|(high&0xff00)
        source=len(out)-distance-1
        require(source >= 0 and len(out)+length <= size, 'ERZ invalid backreference')
        # Every distance denotes the preceding byte plus one; overlapping copy is intentional.
        for j in range(length):
            put(out[source+j])

def table(data, offset=0, limit=None, slack=4, max_count=65535, allow_empty=False):
    limit=len(data) if limit is None else limit
    require(0 <= offset <= limit <= len(data) and limit-offset >= 8, 'table header out of bounds')
    n=u32(data,offset)
    require((0 if allow_empty else 1) <= n <= max_count, 'invalid table count')
    header=4+4*(n+1)
    require(header <= limit-offset, 'table offsets truncated')
    offsets=list(struct.unpack_from('>'+str(n+1)+'I',data,offset+4))
    require(header <= offsets[0] <= header+slack, 'invalid first table offset')
    require(all(a<=b for a,b in zip(offsets,offsets[1:])) and offsets[-1] <= limit-offset, 'table range escapes container')
    return [(offset+a,offset+b) for a,b in zip(offsets,offsets[1:])]

def join_blocks(rom, ranges, block_limit):
    result=[]; provenance=[]; size=0
    for a,b in ranges:
        if a==b:
            continue
        data=rom[a:b]
        decoded=decode_erz(data) if data[:3]==b'ERZ' else data
        require(len(decoded)<=block_limit, 'decoded block exceeds stream-unit size')
        provenance.append({'rom_start':a,'rom_end':b,'stream_start':size,'stream_end':size+len(decoded),'sha256':sha256(decoded)})
        result.append(decoded); size+=len(decoded)
        require(size <= 128*1024*1024, 'stream output limit')
    return b''.join(result),provenance

def boot_base(boot):
    matches=[]
    for p in range(0,len(boot)-24,4):
        if boot[p:p+12] != bytes.fromhex('db02000000000018dc08060a') or u32(boot,p+16)!=0xdc08090a:
            continue
        light, ambient=u32(boot,p+12),u32(boot,p+20)
        if light != ambient+8:
            continue
        bodies=[]
        for q in range(max(0,p-0x400),p-23,4):
            d=boot[q:q+24]
            if any(d[x] for x in (3,7,11,15,19)) or d[:3]!=d[4:7] or d[8:11]!=d[12:15] or not any(d[8:11]):
                continue
            direction=struct.unpack('3b',d[16:19]); length=sum(v*v for v in direction)**.5
            if 120 <= length <= 134:
                bodies.append(q)
        require(len(bodies)==1, 'ambiguous boot light rig')
        matches.append(((ambient&0xdfffffff)-bodies[0],bodies[0],p))
    require(len(matches)==1, 'boot base cannot be proven uniquely')
    base,body,dl=matches[0]
    return base,{'body_offset':body,'display_list_offset':dl,'ambient':list(boot[body:body+3]),'light':list(boot[body+8:body+11]),'direction':list(struct.unpack('3b',boot[body+16:body+19]))}

def bundle_names(boot, base):
    def entry(p):
        pointer,slot=u32(boot,p),u32(boot,p+4)
        q=(pointer&0xdfffffff)-base
        if slot>4096 or q<0 or q>=len(boot):
            return None
        end=boot.find(b'\0',q,min(len(boot),q+64))
        if end<=q or any(c<32 or c>126 for c in boot[q:end]):
            return None
        name=boot[q:end].decode('ascii')
        return (slot,name[:-4],p,q) if name.lower().endswith('.psx') else None
    runs=[]; p=0
    while p+8 <= len(boot):
        run=[]; q=p
        while q+8 <= len(boot):
            e=entry(q)
            if e is None:
                break
            run.append(e); q+=8
        if len(run)>=16:
            runs.append(run); p=q
        else:
            p+=4
    require(len(runs)==1, 'bundle name array must be unique')
    names={}; ambiguous=set()
    for slot,name,p,q in runs[0]:
        if slot in names and names[slot]['name'].lower()!=name.lower():
            ambiguous.add(slot)
        names[slot]={'name':name,'table_offset':p,'string_offset':q}
    return {s:v for s,v in names.items() if s not in ambiguous}

def read_archive(rom, output_dir=None):
    require(len(rom)==ROM_SIZE and sha256(rom)==ROM_SHA256, 'ROM identity mismatch')
    boot_ranges=None
    for p in range(0x1000,len(rom)-12,4):
        if not 1 <= u32(rom,p) <=4095:
            continue
        try:
            spans=table(rom,p,slack=0,max_count=4095)
            if all(b>a for a,b in spans) and any(rom[a:a+4]==b'ERZ\x02' for a,b in spans):
                boot_ranges=spans; break
        except FormatError:
            pass
    require(boot_ranges is not None, 'no strict boot table')
    pointer=u32(rom,p-4)
    require(pointer>>28==11, 'invalid master pointer')
    root=pointer&0x0fffffff
    root_ranges=table(rom,root,slack=16,max_count=64)
    boot,boot_prov=join_blocks(rom,boot_ranges,65536)
    base,light=boot_base(boot)
    names=bundle_names(boot,base)
    groups=[]
    for idx,(a,b) in enumerate(root_ranges):
        leaves=table(rom,a,b,slack=0,max_count=4095,allow_empty=True)
        compressed=any(rom[x:x+4]==b'ERZ\x02' for x,y in leaves if y>x)
        if compressed:
            data,provenance=join_blocks(rom,leaves,16384)
        else:
            data=None; provenance=[]
        groups.append({'index':idx,'directory_offset':a,'leaves':leaves,'compressed':compressed,'data':data,'blocks':provenance})
    result={'boot':boot,'boot_load_base':base,'boot_table_offset':p,'boot_blocks':boot_prov,'root_offset':root,'light':light,'names':names,'groups':groups}
    if output_dir:
        out=Path(output_dir); out.mkdir(parents=True,exist_ok=True)
        (out/'boot.bin').write_bytes(boot)
        for g in groups:
            if g['data'] is not None:
                (out/f"group{g['index']}.bin").write_bytes(g['data'])
    return result
