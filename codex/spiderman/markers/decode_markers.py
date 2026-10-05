"""Extract Spider-Man's boot-authored attachment markers from the exact USA ROM.

No original records or machine-code bytes are embedded here. The source is the
static table installed by the original player constructor, NOT a shell packet.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from codex.spiderman.assets.archive import load_rom, read_archive, require, sha256, table, ROM_SHA256
from codex.spiderman.mesh.decode_spiderman import parse_shell
from codex.spiderman.movement.original_mips import BOOT_SHA256

BOOT_BASE = 0x80016AE0
TABLE_ADDRESS = 0x800F7964
TABLE_OFFSET = TABLE_ADDRESS - BOOT_BASE
SOURCE_ROUTINES = {
    'constructor_installs_table': (0x80094ADC, 0x80094AEC),
    'table_setter': (0x8004C088, 0x8004C0A8),
    'marker_world': (0x8004A720, 0x8004A8F8),
    'joint_matrix_conversion': (0x80051744, 0x800517FC),
    'body_matrix_conversion': (0x800517FC, 0x800518A8),
    'marker_transform': (0x800519F4, 0x80051C58),
    'body_translation_refresh': (0x80049A50, 0x80049AD0),
}

def decode_table(data, offset, joint_count, max_markers=256):
    """Bounded count+8-byte-record decoder, independently unit-testable."""
    require(isinstance(joint_count, int) and 1 <= joint_count <= 65536, 'invalid joint count')
    require(0 <= offset <= len(data)-4, 'marker header out of bounds')
    count = struct.unpack_from('>I', data, offset)[0]
    require(1 <= count <= max_markers, 'invalid marker count')
    end = offset+4+8*count
    require(end <= len(data), 'marker records truncated')
    records = []
    for index in range(count):
        p = offset+4+8*index
        x,y,z,joint = struct.unpack_from('>hhhH', data, p)
        require(joint < joint_count, 'marker joint out of bounds')
        records.append({'index':index,'xyz_s16':[x,y,z],'joint':joint,
                        'source_boot_offset':p,'source_bytes':8,
                        'sha256':sha256(data[p:p+8])})
    return {'count':count,'records':records,'source_range':[offset,end],
            'source_sha256':sha256(data[offset:end])}

def decode_original(rom):
    archive = read_archive(rom)
    boot = archive['boot']
    require(archive['boot_load_base'] == BOOT_BASE and sha256(boot) == BOOT_SHA256,
            'unsupported decoded boot identity')
    names = [slot for slot,entry in archive['names'].items() if entry['name'].lower() == 'spidey']
    require(names == [117], 'original Spider-Man bundle identity mismatch')
    models = next(g['data'] for g in archive['groups'] if g['index'] == 0)
    a,b = table(models)[117]
    bundle = models[a:b]
    parts = table(bundle)
    require(len(parts) == 4, 'unexpected Spider-Man bundle layout')
    sa,sb = parts[2]
    shell_bytes = bundle[sa:sb]
    shell = parse_shell(shell_bytes)
    joint_count = len(shell['objects'])
    decoded = decode_table(boot,TABLE_OFFSET,joint_count)
    require(decoded['count'] == 9, 'unexpected original marker count')
    lo,hi = decoded['source_range']
    provenance = [p for p in archive['boot_blocks'] if p['stream_start'] < hi and p['stream_end'] > lo]
    return {'schema':'n64codexlab.spiderman.markers.v1',
            'source':{'rom_sha256':ROM_SHA256,'boot_sha256':BOOT_SHA256,'boot_load_base':BOOT_BASE,
                      'table_address':TABLE_ADDRESS,'table_boot_range':[lo,hi],
                      'table_sha256':decoded['source_sha256'],'boot_compressed_blocks':provenance,
                      'model_slot':117,'model_joint_count':joint_count,
                      'model_shell_sha256':sha256(shell_bytes),
                      'routines':{name:{'virtual_range':[x,y],
                                      'sha256':sha256(boot[x-BOOT_BASE:y-BOOT_BASE])}
                                  for name,(x,y) in SOURCE_ROUTINES.items()}},
            'count':decoded['count'],'markers':decoded['records'],
            'contract':{'source':'static boot table; constructor passes count header to setter, which skips four bytes',
                        'record':'3 big-endian signed16 authored coordinates + big-endian unsigned16 joint',
                        'pose':'12 signed16: row-major fixed12 rotation, then authored integer translation',
                        'body_translation':'retained integer matrix translation, separate from current body position',
                        'position':'native signed32 fixed12 world coordinates',
                        'evaluation':'binary32 joint transform then binary32 body transform, trunc toward zero; signed32 wrapped <<12 and body_position + arithmetic_shift4(transformed_fixed12-body_position)',
                        'mirror':'negate signed16 body matrix first column; inputs restored by original routine',
                        'unsupported':'does not select animations, generate compressed poses, or name unverified marker semantics'}}

def export(rom_path, output):
    result = decode_original(load_rom(rom_path))
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result,indent=2)+'\n')
    return result

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom')
    parser.add_argument('--out',default='codex/.runtime/spiderman-assets/original-markers.json')
    args=parser.parse_args()
    result=export(args.rom,args.out)
    print(json.dumps({'marker_count':result['count'],'joint_count':result['source']['model_joint_count'],
                      'table_sha256':result['source']['table_sha256'],'output':str(args.out)},indent=2))
