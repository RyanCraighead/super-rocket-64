#!/usr/bin/env python3
"""Read-only, revision-locked extraction of user-supplied original BK assets.

Accepts a local N64 image or a ZIP containing exactly one N64 image. No network,
third-party executable, ROM patching or original full-ROM output is performed.
"""
from __future__ import annotations
import hashlib
import os
from pathlib import Path
import stat
import struct
import zipfile
import zlib

ROM_SIZE = 0x1000000
EXPECTED_SHA1 = 'ded6ee166e740ad1bc810fd678a84b48e245ab80'
EXPECTED_SHA256 = 'f9ad43f64c3a38b0ca4067e24f570dcb8f3f8fec077c5c342484bcbec7ff6d22'
TABLE_OFFSET = 0x5E90
ASSET_COUNT = 0xE24
MAX_DECODED = 2 * 1024 * 1024
MAX_ZIP = 32 * 1024 * 1024
MODEL_ID = 0x34E

class InputError(ValueError):
    pass

def require(condition, message):
    if not condition:
        raise InputError(message)

def normalize(data):
    require(isinstance(data, bytes) and len(data) == ROM_SIZE,
            'Expected a 16,777,216-byte original retail ROM')
    if data[:4] == b'\x80\x37\x12\x40':
        return data, 'z64'
    out = bytearray(ROM_SIZE)
    if data[:4] == b'\x37\x80\x40\x12':
        out[0::2], out[1::2] = data[1::2], data[0::2]
        return bytes(out), 'v64'
    if data[:4] == b'\x40\x12\x37\x80':
        for i in range(4):
            out[i::4] = data[3-i::4]
        return bytes(out), 'n64'
    raise InputError('Unknown N64 byte order; copier headers are unsupported')

def validate(data):
    rom, order = normalize(data)
    sha1, sha256 = hashlib.sha1(rom).hexdigest(), hashlib.sha256(rom).hexdigest()
    require(sha1 == EXPECTED_SHA1 and sha256 == EXPECTED_SHA256,
            'Unsupported ROM; required Banjo-Kazooie (USA) (Rev 1), normalized SHA-1 '
            + EXPECTED_SHA1 + '; actual ' + sha1)
    require(rom[0x3b:0x40] == b'NBKE\x01' and
            rom[0x10:0x18] == bytes.fromhex('cd7559acb26cf5ae'), 'ROM revision header mismatch')
    return rom, dict(profile='bk-us-rev1',size=len(rom),input_byte_order=order,
                     normalized_sha1=sha1, normalized_sha256=sha256,
                     normalized_crc32=f'{zlib.crc32(rom):08x}')

def read_rom(path):
    path = Path(path)
    before = path.stat()
    require(stat.S_ISREG(before.st_mode) and 0 < before.st_size <= MAX_ZIP,
            'Input must be a bounded regular local ROM or ZIP file')
    fd = os.open(path, os.O_RDONLY | getattr(os,'O_NONBLOCK',0) | getattr(os,'O_BINARY',0))
    with os.fdopen(fd, 'rb') as stream:
        info = os.fstat(stream.fileno())
        require(stat.S_ISREG(info.st_mode) and info.st_size == before.st_size and
                (info.st_dev,info.st_ino)==(before.st_dev,before.st_ino), 'Input changed while opening')
        magic = stream.read(4)
        stream.seek(0)
        if magic[:2] == b'PK':
            try:
                with zipfile.ZipFile(stream) as archive:
                    infos = archive.infolist()
                    require(len(infos) <= 32,'ZIP member limit exceeded')
                    members = [i for i in infos if not i.is_dir() and
                               Path(i.filename).suffix.lower() in ('.z64','.v64','.n64')]
                    require(len(members)==1,'ZIP must contain exactly one N64 image')
                    member=members[0]
                    require(member.file_size==ROM_SIZE and not member.flag_bits&1,
                            'ZIP ROM must be unencrypted and exactly 16 MiB')
                    require(member.compress_type in (zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED),
                            'Unsupported ZIP compression')
                    with archive.open(member) as source:
                        data=source.read(ROM_SIZE+1)
            except (zipfile.BadZipFile,RuntimeError,NotImplementedError,zlib.error,EOFError) as error:
                raise InputError('Invalid ROM ZIP: '+str(error)) from error
        else:
            require(info.st_size==ROM_SIZE,'Input ROM must be exactly 16 MiB')
            data=stream.read(ROM_SIZE+1)
    require(len(data)==ROM_SIZE,'ROM length changed during reading')
    return data

def decode_rarezip(source):
    require(isinstance(source,bytes) and 7 <= len(source) <= MAX_DECODED,
            'Invalid bounded RareZip input')
    require(source[:2]==b'\x11\x72','Missing original RareZip signature')
    expected=struct.unpack_from('>I',source,2)[0]
    require(0 < expected <= MAX_DECODED,'RareZip decoded size limit')
    decoder=zlib.decompressobj(-15)
    try:
        output=decoder.decompress(source[6:],expected+1)
    except zlib.error as error:
        raise InputError('Invalid RareZip DEFLATE stream') from error
    require(len(output)==expected and decoder.eof and not decoder.unconsumed_tail,
            'Truncated or oversized RareZip stream')
    # Original assets are eight-byte aligned with 0xAA padding, never another stream.
    require(len(decoder.unused_data)<=7 and all(v==0xaa for v in decoder.unused_data),
            'Unexpected bytes after RareZip stream')
    return output

def extract_asset(rom, asset_id):
    require(isinstance(rom,bytes) and len(rom)==ROM_SIZE and
            hashlib.sha256(rom).hexdigest()==EXPECTED_SHA256,
            'Asset extraction requires authenticated original USA Rev1 ROM')
    require(type(asset_id) is int and 0 <= asset_id < ASSET_COUNT-1,'Invalid asset ID')
    require(struct.unpack_from('>II',rom,TABLE_OFFSET)==(ASSET_COUNT,0xffffffff),
            'Unexpected original asset table')
    relative,flags,kind=struct.unpack_from('>IHH',rom,TABLE_OFFSET+8+asset_id*8)
    end=struct.unpack_from('>I',rom,TABLE_OFFSET+8+(asset_id+1)*8)[0]
    base=TABLE_OFFSET+8+ASSET_COUNT*8
    require(flags in (0,1) and relative <= end and 0 < end-relative <= MAX_DECODED
            and base+end <= len(rom),'Invalid asset entry or range')
    stored=rom[base+relative:base+end]
    decoded=decode_rarezip(stored) if flags else stored
    return decoded,dict(asset_id=asset_id,table_rom=TABLE_OFFSET+8+asset_id*8,
                        rom_offset=base+relative,stored_size=len(stored),
                        compressed=bool(flags),kind=kind,decoded_size=len(decoded),
                        sha256=hashlib.sha256(decoded).hexdigest())
