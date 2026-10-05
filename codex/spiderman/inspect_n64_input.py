#!/usr/bin/env python3
"""Read-only intake metadata for a user-supplied N64 image or single-image ZIP.

This does not assert a supported Spider-Man revision or extract game assets.
No network, subprocess, archive extraction or input mutation is performed.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import struct
import zipfile
import zlib

MAX_INPUT = 70 * 1024 * 1024
MAX_ROM = 64 * 1024 * 1024
MIN_ROM = 4 * 1024 * 1024
ROM_SUFFIXES = {'.z64', '.v64', '.n64'}


class InputError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise InputError(message)


def normalize(data: bytes) -> tuple[bytes, str]:
    require(isinstance(data, bytes) and MIN_ROM <= len(data) <= MAX_ROM
            and len(data) % MIN_ROM == 0, 'Expected a bounded N64 image in 4 MiB units')
    if data[:4] == b'\x80\x37\x12\x40':
        return data, 'z64'
    out = bytearray(len(data))
    if data[:4] == b'\x37\x80\x40\x12':
        out[0::2], out[1::2] = data[1::2], data[0::2]
        return bytes(out), 'v64'
    if data[:4] == b'\x40\x12\x37\x80':
        for i in range(4):
            out[i::4] = data[3 - i::4]
        return bytes(out), 'n64'
    raise InputError('Unknown N64 byte order; copier headers are unsupported')


def read_input(path: Path) -> tuple[bytes, str | None, str]:
    before = path.stat()
    require(stat.S_ISREG(before.st_mode) and 0 < before.st_size <= MAX_INPUT,
            'Input must be a bounded regular local ROM or ZIP')
    fd = os.open(path, os.O_RDONLY | getattr(os, 'O_NONBLOCK', 0)
                 | getattr(os, 'O_BINARY', 0))
    with os.fdopen(fd, 'rb') as stream:
        info = os.fstat(stream.fileno())
        require(stat.S_ISREG(info.st_mode)
                and (info.st_dev, info.st_ino, info.st_size)
                == (before.st_dev, before.st_ino, before.st_size), 'Input changed while opening')
        original = stream.read(MAX_INPUT + 1)
        require(len(original) == info.st_size, 'Input length changed while reading')
        original_hash = hashlib.sha256(original).hexdigest()
        if original[:2] != b'PK':
            require(len(original) <= MAX_ROM, 'ROM exceeds size limit')
            return original, None, original_hash
        try:
            with zipfile.ZipFile(io.BytesIO(original)) as archive:
                infos = archive.infolist()
                require(len(infos) <= 32, 'ZIP member limit exceeded')
                members = [item for item in infos if not item.is_dir()
                           and Path(item.filename).suffix.lower() in ROM_SUFFIXES]
                require(len(members) == 1, 'ZIP must contain exactly one N64 image')
                item = members[0]
                require(MIN_ROM <= item.file_size <= MAX_ROM
                        and item.file_size % MIN_ROM == 0, 'Invalid ROM size in ZIP')
                require(not item.flag_bits & 1, 'Encrypted ZIP is unsupported')
                require(item.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED),
                        'Unsupported ZIP compression')
                with archive.open(item) as source:
                    data = source.read(MAX_ROM + 1)
                require(len(data) == item.file_size, 'ZIP ROM length mismatch')
                return data, item.filename, original_hash
        except (zipfile.BadZipFile, RuntimeError, NotImplementedError, zlib.error, EOFError, UnicodeError) as error:
            raise InputError('Invalid ROM ZIP: ' + str(error)) from error


def printable(data: bytes) -> str:
    return ''.join(chr(byte) if 32 <= byte < 127 else '?' for byte in data.rstrip(b' \0'))


def inspect(path: Path) -> dict:
    data, member, source_hash = read_input(path)
    rom, order = normalize(data)
    crc1, crc2 = struct.unpack_from('>II', rom, 0x10)
    return {
        'status': 'identified-header-only',
        'supported_for_gameplay': False,
        'note': 'Header text is not proof of a supported retail revision. Match complete hashes before extraction.',
        'input_name': path.name,
        'input_sha256': source_hash,
        'archive_member': member,
        'rom_size': len(rom),
        'input_byte_order': order,
        'header_title': printable(rom[0x20:0x34]),
        'header_serial': printable(rom[0x3B:0x3F]),
        'header_country_byte': rom[0x3E],
        'header_revision': rom[0x3F],
        'header_crc1': f'{crc1:08x}',
        'header_crc2': f'{crc2:08x}',
        'normalized_sha1': hashlib.sha1(rom).hexdigest(),
        'normalized_sha256': hashlib.sha256(rom).hexdigest(),
        'normalized_crc32': f'{zlib.crc32(rom):08x}',
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    args = parser.parse_args()
    try:
        report = inspect(args.input)
    except (InputError, OSError) as error:
        parser.exit(2, 'Input rejected: ' + str(error) + '\n')
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
