"""Bounded reader/evaluator for original Banjo-Kazooie animation assets.

Format and evaluation evidence: n64decomp/banjo-kazooie commit
 e8d31fa53645616fcac6e051543222b973a2edb0, include/core2/animationfile.h,
 src/core2/animationfile.c, code_B9770.c (Catmull-Rom), code_630D0.c
 (pivot palettes), code_BE2C0.c (Euler/quaternion conversion), core1/mlmtx.c.
No poses or game data are embedded. Decoded values remain signed Q10.6 keys.
The evaluator ports the original per-track interpolation, including its two
independent tangent flags, default-to-first-key interval and final-key tail.
Matrix math uses host libm rather than the original quaternion/trig/RSP
rounding. Frames are source frame numbers, not a fixed frames-per-second rate.
"""
from __future__ import annotations

import hashlib
import math
import struct

BONE_COUNT = 0x6D
MAX_ASSET_SIZE = 2 * 1024 * 1024
IDENTITY_TRANSFORM = (0., 0., 0., 0., 0., 0., 1., 1., 1.)
IDENTITY_MATRIX = (1., 0., 0., 0., 0., 1., 0., 0.,
                   0., 0., 1., 0., 0., 0., 0., 1.)


class AnimationFormatError(ValueError):
    """Malformed or unsupported original animation/model data."""


def _need(condition, message):
    if not condition:
        raise AnimationFormatError(message)


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def _unpack(fmt, data, offset):
    size = struct.calcsize('>' + fmt)
    _need(0 <= offset <= len(data) - size, 'Truncated animation/model record')
    return struct.unpack_from('>' + fmt, data, offset)


def decode_animation(data: bytes, asset_id: int, name: str) -> dict:
    """Decode one decompressed original animation asset, failing closed.

    A key is [frame, signed_value_raw, flags]; flags bit0 selects the prior
    tangent and bit1 selects the next tangent, as in the two original bitfields.
    """
    _need(type(asset_id) is int and 0 <= asset_id <= 0xFFFF,
          'Invalid animation asset ID')
    _need(isinstance(name, str) and 0 < len(name) <= 128,
          'Invalid animation name')
    _need(8 <= len(data) <= MAX_ASSET_SIZE, 'Animation asset size outside bounds')
    first, last, count, pad = _unpack('hhhH', data, 0)
    _need(0 <= first <= last <= 0x3FFF and pad == 0,
          'Invalid animation header')
    _need(1 <= count <= (BONE_COUNT - 1) * 9, 'Invalid animation element count')
    offset, previous_bone, seen, tracks = 8, 0, set(), []
    for _ in range(count):
        word, key_count = _unpack('Hh', data, offset)
        offset += 4
        bone, channel = word >> 4, word & 15
        _need(1 <= bone < BONE_COUNT and bone >= previous_bone,
              'Invalid or unordered animation bone')
        _need(0 <= channel < 9 and (bone, channel) not in seen,
              'Invalid or duplicate animation channel')
        _need(1 <= key_count <= 0x4000 and key_count <= (len(data) - offset) // 4,
              'Invalid or truncated animation keys')
        seen.add((bone, channel))
        previous_bone = bone
        keys, previous_frame = [], first - 1
        for _ in range(key_count):
            packed, value = _unpack('Hh', data, offset)
            offset += 4
            frame, flags = packed & 0x3FFF, packed >> 14
            _need(first <= frame <= last and frame > previous_frame,
                  'Unordered or out-of-range animation key')
            keys.append([frame, value, flags])
            previous_frame = frame
        tracks.append({'bone': bone, 'channel': channel, 'keys': keys})
    _need(offset == len(data), 'Unexpected trailing animation data')
    return {'id': asset_id, 'name': name, 'first_frame': first,
            'last_frame': last, 'duration': last - first, 'tracks': tracks,
            'source_size': len(data), 'source_sha256': hashlib.sha256(data).hexdigest()}


def parse_model_skeleton(model: bytes) -> dict:
    """Read BKModelBin.animation_list_offset and original flat matrix palette.

    The original vertices are in model space; the stored translation is a
    pivot, not a bind-pose bone offset. Each mtx_id refers backward in palette
    order (or -1 for the actor matrix), allowing multiple roots.
    """
    _need(0x38 <= len(model) <= 16 * 1024 * 1024, 'Model size outside bounds')
    magic, = _unpack('I', model, 0)
    _need(magic == 0xB, 'Unsupported BK model signature')
    offset, = _unpack('I', model, 0x18)
    _need(0x38 <= offset <= len(model) - 8 and offset % 4 == 0,
          'Invalid skeleton offset')
    scale, count, pad = _unpack('fhH', model, offset)
    _need(math.isfinite(scale) and 0 < scale <= 1000 and pad == 0,
          'Invalid skeleton translation scale/header')
    _need(1 <= count <= BONE_COUNT and count <= (len(model) - offset - 8) // 16,
          'Invalid skeleton matrix count')
    bones = []
    for index in range(count):
        x, y, z, bone, parent = _unpack('fffhh', model, offset + 8 + index * 16)
        _need(all(math.isfinite(v) and abs(v) <= 100000 for v in (x, y, z)),
              'Nonfinite or unbounded skeleton pivot')
        _need(0 <= bone < BONE_COUNT and -1 <= parent < index,
              'Invalid bone ID or forward/cyclic matrix reference')
        bones.append({'index': index, 'bone_id': bone, 'parent': parent,
                      'pivot': [x, y, z]})
    return {'translation_scale': scale, 'bones': bones}


def catmull_rom(x, knots):
    """Four-knot specialization of glspline_catmull_rom_interpolate."""
    x = f32(max(0., min(1., x)))
    a, b, c, d = knots
    c2 = f32(-.5 * a + 1.5 * b - 1.5 * c + .5 * d)
    c1 = f32(a - 2.5 * b + 2. * c - .5 * d)
    c0 = f32(-.5 * a + .5 * c)
    return f32(f32(f32(f32(f32(f32(c2 * x) + c1) * x) + c0) * x) + b)


def sample_track(animation: dict, track: dict, frame: float) -> float:
    """Source animationfilebin_func_8033AC38, on already decoded records."""
    keys = track['keys']
    first, last = keys[0], keys[-1]
    if int(frame) < first[0]:
        default = 1. if 3 <= track['channel'] <= 5 else 0.
        after = keys[1][1] / 64. if first[2] & 2 and len(keys) >= 2 else first[1] / 64.
        t = f32(f32(frame - animation['first_frame']) /
                (first[0] - animation['first_frame']))
        return catmull_rom(t, (default, default, first[1] / 64., after))
    if int(frame) >= last[0]:
        value = last[1] / 64.
        before = keys[-2][1] / 64. if last[2] & 1 and len(keys) >= 2 else value
        return catmull_rom(f32(frame - last[0]), (before, value, value, value))
    low, high = 0, len(keys) - 1
    while high - low > 1:
        mid = (low + high) // 2
        if keys[mid][0] <= int(frame):
            low = mid
        else:
            high = mid
    left, right = keys[low], keys[high]
    a, b = left[1] / 64., right[1] / 64.
    t = f32(f32(frame - left[0]) / (right[0] - left[0]))
    if not left[2] & 1 and not right[2] & 2:
        return f32(a + f32(f32(b - a) * t))
    before = keys[low - 1][1] / 64. if left[2] & 1 and low else a
    after = keys[high + 1][1] / 64. if right[2] & 2 and high + 1 < len(keys) else b
    return catmull_rom(t, (before, a, b, after))


def sample_animation(animation: dict, frame: float) -> tuple:
    _need(isinstance(frame, (int, float)) and not isinstance(frame, bool)
          and math.isfinite(frame)
          and animation['first_frame'] <= frame <= animation['last_frame'],
          'Frame outside original animation interval')
    frame = f32(frame)
    pose = [list(IDENTITY_TRANSFORM) for _ in range(BONE_COUNT)]
    # Source track order is rotation, scale, translation. The public transform
    # order follows the existing renderer contract: translation, rotation, scale.
    mapping = (3, 4, 5, 6, 7, 8, 0, 1, 2)
    for track in animation['tracks']:
        pose[track['bone']][mapping[track['channel']]] = sample_track(animation, track, frame)
    return tuple(tuple(t) for t in pose)


def sample_progress(animation: dict, progress: float) -> tuple:
    _need(isinstance(progress, (int, float)) and not isinstance(progress, bool)
          and math.isfinite(progress) and 0 <= progress <= 1,
          'Progress outside original animation interval')
    frame = f32(animation['first_frame'] + f32(f32(progress) * animation['duration']))
    return sample_animation(animation, frame)


def multiply(a, b):
    """Column-major matrix multiplication."""
    return tuple(sum(a[k * 4 + row] * b[col * 4 + k] for k in range(4))
                 for col in range(4) for row in range(4))


def translation(v):
    m = list(IDENTITY_MATRIX)
    m[12:15] = v
    return tuple(m)


def rotation_matrix(rotation):
    x, y, z = (v * (3.141592654 / 180.) for v in rotation)
    cx, cy, cz, sx, sy, sz = math.cos(x), math.cos(y), math.cos(z), math.sin(x), math.sin(y), math.sin(z)
    rx = (1., 0., 0., 0., 0., cx, sx, 0., 0., -sx, cx, 0., 0., 0., 0., 1.)
    ry = (cy, 0., -sy, 0., 0., 1., 0., 0., sy, 0., cy, 0., 0., 0., 0., 1.)
    rz = (cz, sz, 0., 0., -sz, cz, 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)
    return multiply(rz, multiply(ry, rx))


def model_palettes(skeleton: dict, pose: tuple) -> tuple:
    """Original model-space pivot matrices plus a final identity sentinel."""
    _need(len(pose) == BONE_COUNT and all(len(t) == 9 for t in pose), 'Invalid pose size')
    _need(all(math.isfinite(v) and abs(v) <= 100000 for t in pose for v in t),
          'Invalid pose scalar')
    result = []
    for bone in skeleton['bones']:
        t, p = pose[bone['bone_id']], bone['pivot']
        moved = [p[k] + skeleton['translation_scale'] * t[k] for k in range(3)]
        scale = list(IDENTITY_MATRIX)
        scale[0], scale[5], scale[10] = t[6:9]
        local = multiply(translation(moved), multiply(rotation_matrix(t[3:6]),
                         multiply(scale, translation([-v for v in p]))))
        result.append(multiply(IDENTITY_MATRIX if bone['parent'] < 0 else result[bone['parent']], local))
    return tuple(result) + (IDENTITY_MATRIX,)
