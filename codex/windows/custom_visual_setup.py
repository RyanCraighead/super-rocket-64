"""Generate exact runtime visuals from pinned original source inputs.

PNG filtering follows the PNG format. N64 pixel quantization matches
tools/n64graphics.c (queueRAM, MIT; see visual_inputs/sm64tools.LICENSE).
This changes packaging, not artwork or the applicable resource rights.
"""
from __future__ import annotations
import hashlib
import re
import struct
import zlib
from pathlib import Path

MAX_INPUT = 8 * 1024 * 1024
MAX_PIXELS = 4 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def rgba_png(raw, cancel_check=lambda: None):
    """Decode the non-interlaced 8-bit RGBA/indexed formats in the source set."""
    require(len(raw) <= MAX_INPUT and raw[:8] == b'\x89PNG\r\n\x1a\n', 'Invalid source PNG signature or size.')
    offset = 8
    header = None
    palette = None
    alpha = None
    compressed = bytearray()
    finished = False
    idat_finished = False
    while offset < len(raw):
        cancel_check()
        require(offset + 12 <= len(raw), 'Truncated source PNG chunk.')
        count, kind = struct.unpack_from('>I4s', raw, offset)
        end = offset + count + 12
        require(count <= MAX_INPUT and end <= len(raw), 'Invalid source PNG chunk length.')
        content = raw[offset + 8:end - 4]
        crc = struct.unpack_from('>I', raw, end - 4)[0]
        require(zlib.crc32(kind + content) & 0xffffffff == crc, 'Source PNG checksum failed.')
        if header is None:
            require(kind == b'IHDR' and count == 13, 'Source PNG header is missing.')
        if kind == b'IHDR':
            require(header is None and count == 13, 'Duplicate or invalid PNG header.')
            header = struct.unpack('>IIBBBBB', content)
            w, h, depth, color, compression, filtering, interlace = header
            require(0 < w <= 8192 and 0 < h <= 8192 and w * h <= MAX_PIXELS,
                    'Unsupported PNG dimensions.')
            require(depth == 8 and color in (3, 6) and compression == filtering == interlace == 0,
                    'Unsupported source PNG format; restore the matching installer.')
        elif kind == b'PLTE':
            require(palette is None and not compressed and count and count <= 768 and count % 3 == 0,
                    'Invalid source PNG palette.')
            palette = content
        elif kind == b'tRNS':
            require(alpha is None and palette is not None and not compressed and color == 3 and
                    0 < count <= len(palette) // 3, 'Invalid source PNG transparency.')
            alpha = content
        elif kind == b'IDAT':
            require(not idat_finished, 'Non-contiguous source PNG image data.')
            compressed.extend(content)
        elif kind == b'IEND':
            require(count == 0 and compressed and end == len(raw), 'Invalid source PNG ending.')
            finished = True
            break
        else:
            require(kind[0] & 32, 'Unsupported critical source PNG chunk.')
            if compressed:
                idat_finished = True
        offset = end
    require(finished and header is not None, 'Incomplete source PNG.')
    channels = 4 if color == 6 else 1
    stride = w * channels
    expected = (stride + 1) * h
    inflater = zlib.decompressobj()
    decoded = inflater.decompress(compressed, expected + 1)
    require(len(decoded) == expected and inflater.eof and not inflater.unused_data and not inflater.unconsumed_tail,
            'Source PNG decoded length failed validation.')
    result = bytearray(stride * h)
    previous = bytearray(stride)
    for y in range(h):
        cancel_check()
        start = y * (stride + 1)
        mode = decoded[start]
        require(mode <= 4, 'Invalid source PNG row filter.')
        row = bytearray(decoded[start + 1:start + stride + 1])
        if mode == 1:
            for x in range(channels, stride):
                row[x] = (row[x] + row[x - channels]) & 255
        elif mode == 2:
            for x in range(stride):
                row[x] = (row[x] + previous[x]) & 255
        elif mode == 3:
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                row[x] = (row[x] + ((left + previous[x]) // 2)) & 255
        elif mode == 4:
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                above = previous[x]
                upper_left = previous[x - channels] if x >= channels else 0
                p = left + above - upper_left
                a, b, c = abs(p - left), abs(p - above), abs(p - upper_left)
                predictor = left if a <= b and a <= c else above if b <= c else upper_left
                row[x] = (row[x] + predictor) & 255
        result[y * stride:(y + 1) * stride] = row
        previous = row
    if color == 6:
        return bytes(result)
    require(palette is not None, 'Indexed source PNG has no palette.')
    alpha = alpha or b''
    colors = [palette[i:i + 3] + bytes([alpha[i // 3] if i // 3 < len(alpha) else 255])
              for i in range(0, len(palette), 3)]
    rgba = bytearray()
    for start in range(0, len(result), w):
        cancel_check()
        for index in result[start:start + w]:
            require(index < len(colors), 'Source PNG palette index is out of range.')
            rgba.extend(colors[index])
    return bytes(rgba)


def encode_pixels(rgba, encoding, cancel_check=lambda: None):
    require(encoding in ('rgba16', 'rgba32', 'ia16', 'ia8'), 'Unsupported runtime texture format.')
    if encoding == 'rgba32':
        return rgba
    result = bytearray()
    for base in range(0, len(rgba), 16384):
        cancel_check()
        for r, g, b, a in struct.iter_unpack('BBBB', rgba[base:base + 16384]):
            if encoding == 'rgba16':
                r, g, b = ((x + 4) * 31 // 255 for x in (r, g, b))
                result.extend(((r << 3) | (g >> 2), ((g & 3) << 6) | (b << 1) | bool(a)))
            else:
                intensity = (r + g + b + 1) // 3
                if encoding == 'ia16':
                    result.extend((intensity, a))
                else:
                    result.append((intensity // 17 << 4) | a // 17)
    return bytes(result)


def generate(root, entry, cancel_check=lambda: None):
    root = Path(root).resolve()
    relative = Path(entry['source_input'])
    require(not relative.is_absolute() and '..' not in relative.parts, 'Invalid visual source path.')
    path = (root / relative).resolve()
    require(path.is_relative_to(root), 'Visual source escapes the installer.')
    cancel_check()
    require(path.is_file() and path.stat().st_size == entry['source_size'] and entry['source_size'] <= MAX_INPUT,
            'Original visual source is missing or has the wrong size: ' + relative.as_posix())
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == entry['source_sha256'],
            'Original visual source changed: ' + relative.as_posix())
    if entry['method'] == 'source_png':
        result = encode_pixels(rgba_png(raw, cancel_check), entry['encoding'], cancel_check)
    else:
        require(entry['method'] == 'source_array', 'Unsupported visual source method.')
        symbol = entry['source_symbol']
        require(re.fullmatch(r'apparition_texture_[1-4]', symbol), 'Invalid source array name.')
        pattern = r'static ALIGNED8 const Texture ' + symbol + r'\[\] = \{([^}]+)\};'
        matches = re.findall(pattern, raw.decode('utf-8'))
        require(len(matches) == 1, 'Original source texture array is missing or ambiguous.')
        body = matches[0]
        require(re.fullmatch(r'\s*0x[0-9A-Fa-f]{1,2}(?:\s*,\s*0x[0-9A-Fa-f]{1,2})*\s*,?\s*', body),
                'Invalid source texture literals.')
        result = bytes(int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]{1,2}', body))
    cancel_check()
    require(len(result) == entry['size'] and hashlib.sha256(result).hexdigest() == entry['sha256'],
            'Generated visual failed exact validation: ' + relative.as_posix())
    return result
