#!/usr/bin/env python3
"""Original, dependency-free, strictly versioned local OoT asset extractor.

No network operations; no assets or ROM bytes are included in this program.
All generated game data stays in a user-selected local output directory.
"""
from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import zlib

HERE = Path(__file__).resolve().parent
SOURCE_REVISION = "52a510f379afd143aaa0375be9f1e190369572e1"
DMA_OFFSET = 0x7430
FRAME_STRIDE = 134
MAX_ROM_SIZE = 64 * 1024 * 1024
MAX_ASSET_SIZE = 16 * 1024 * 1024
# Hashes refer to normalized (big-endian) complete files, not header checksums.
ROM_PROFILES = {
    "5bd1fe107bf8106b2ab6650abecd54d6": ("ntsc-1.0-US", 0, "US NTSC 1.0 compressed retail"),
    "6829a16db1a34e8ce989847cd8da8d9a": ("ntsc-1.0-US", 0, "US NTSC 1.0 zeldaret decompressed"),
    "57a9719ad547c516342e1a15d5c28c3d": ("ntsc-1.2-US", 2, "US NTSC 1.2 compressed retail"),
    "12fcafeba93992facaf65c2ba00f3089": ("ntsc-1.2-US", 2, "US NTSC 1.2 zeldaret decompressed"),
}
METADATA_FILES = {
    "ntsc-1.0-US": "oot_ntsc_10_metadata.json",
    "ntsc-1.2-US": "oot_ntsc_12_metadata.json",
}


class AssetError(ValueError):
    """An invalid or unsupported input; never silently proceed."""


def require(condition, message):
    if not condition:
        raise AssetError(message)


def checked_slice(data, offset, size, context="data"):
    require(offset >= 0 and size >= 0 and offset <= len(data) and size <= len(data) - offset,
            f"{context}: range 0x{offset:X}+0x{size:X} exceeds 0x{len(data):X} bytes")
    return data[offset:offset + size]


def normalize_rom(data):
    require(4 <= len(data) <= MAX_ROM_SIZE, "ROM size must be between 4 bytes and 64 MiB")
    magic = data[:4]
    if magic == bytes.fromhex("80371240"):
        return bytes(data), "z64-big-endian"
    if magic == bytes.fromhex("37804012"):
        require(len(data) % 2 == 0, "v64 ROM has an incomplete 16-bit word")
        out = bytearray(len(data))
        out[::2], out[1::2] = data[1::2], data[::2]
        return bytes(out), "v64-byte-swapped"
    if magic == bytes.fromhex("40123780"):
        require(len(data) % 4 == 0, "n64 ROM has an incomplete 32-bit word")
        out = bytearray(len(data))
        for byte in range(4):
            out[byte::4] = data[3 - byte::4]
        return bytes(out), "n64-little-endian"
    raise AssetError("Not a recognized N64 ROM byte order (z64/v64/n64)")


def identify_rom(data):
    rom, byte_order = normalize_rom(data)
    digest = hashlib.md5(rom).hexdigest()
    require(digest in ROM_PROFILES,
            f"Unsupported normalized ROM MD5 {digest}; exact US OoT 1.0 or 1.2 required. No bytes extracted.")
    profile, revision, description = ROM_PROFILES[digest]
    require(rom[0x3E:0x40] == bytes((0x45, revision)),
            f"Expected US country E and revision {revision}")
    return rom, {"version": description, "profile": profile, "header_revision": revision,
                 "normalized_md5": digest, "normalized_sha1": hashlib.sha1(rom).hexdigest(),
                 "normalized_sha256": hashlib.sha256(rom).hexdigest(),
                 "size": len(rom), "input_byte_order": byte_order}


def decode_yaz0(data, expected_size=None, max_output=MAX_ASSET_SIZE):
    """Decode literal/run groups with bounded, overlapping back-references."""
    require(len(data) >= 16 and data[:4] == b"Yaz0", "Missing Yaz0 header")
    size = struct.unpack_from(">I", data, 4)[0]
    require(size <= max_output, "Yaz0 output exceeds configured limit")
    if expected_size is not None:
        require(size == expected_size, "Yaz0 header size disagrees with DMA virtual span")
    cursor, result = 16, bytearray()
    while len(result) < size:
        require(cursor < len(data), "Truncated Yaz0 control group")
        control = data[cursor]
        cursor += 1
        for bit in range(7, -1, -1):
            if len(result) == size:
                break
            if control & (1 << bit):
                require(cursor < len(data), "Truncated Yaz0 literal")
                result.append(data[cursor])
                cursor += 1
            else:
                require(cursor + 2 <= len(data), "Truncated Yaz0 back-reference")
                first, second = data[cursor:cursor + 2]
                cursor += 2
                distance = ((first & 15) << 8 | second) + 1
                length = first >> 4
                if length:
                    length += 2
                else:
                    require(cursor < len(data), "Truncated Yaz0 extended length")
                    length = data[cursor] + 18
                    cursor += 1
                require(distance <= len(result), "Yaz0 reference precedes output buffer")
                require(length <= size - len(result), "Yaz0 run exceeds declared output size")
                for _ in range(length):
                    result.append(result[-distance])
    return bytes(result)


@dataclasses.dataclass(frozen=True)
class DmaEntry:
    index: int
    vrom_start: int
    vrom_end: int
    rom_start: int
    rom_end: int

    @property
    def size(self):
        return self.vrom_end - self.vrom_start

    def extract(self, rom):
        require(self.rom_start != 0xFFFFFFFF and self.rom_end != 0xFFFFFFFF,
                f"DMA {self.index}: deleted file")
        require(0 < self.size <= MAX_ASSET_SIZE, f"DMA {self.index}: invalid virtual span")
        if self.rom_end == 0:
            return checked_slice(rom, self.rom_start, self.size, f"DMA {self.index}")
        require(self.rom_end > self.rom_start, f"DMA {self.index}: invalid compressed span")
        packed = checked_slice(rom, self.rom_start, self.rom_end - self.rom_start, f"DMA {self.index}")
        return decode_yaz0(packed, self.size)


def read_dma_table(rom, offset=DMA_OFFSET, max_entries=2048):
    entries = []
    for index in range(max_entries):
        raw = checked_slice(rom, offset + index * 16, 16, "DMA table")
        fields = struct.unpack(">IIII", raw)
        if fields == (0, 0, 0, 0):
            require(entries, "DMA table is empty")
            return entries
        entry = DmaEntry(index, *fields)
        require(entry.vrom_end > entry.vrom_start, f"DMA {index}: inverted virtual range")
        if entries:
            require(entry.vrom_start >= entries[-1].vrom_end,
                    f"DMA {index}: overlapping or unsorted virtual ranges")
        entries.append(entry)
    raise AssetError("DMA table missing bounded terminator")


def segment_offset(pointer, segment, size, needed=0):
    require(pointer >> 24 == segment, f"Expected segment {segment:02X}, got 0x{pointer:08X}")
    offset = pointer & 0xFFFFFF
    require(offset <= size and needed <= size - offset, "Segmented pointer exceeds asset")
    return offset


def decode_skeleton(data, spec):
    symbols = spec["symbols"]
    header = next(s for s in symbols if s["kind"] == "Skeleton")
    offset = header["offset"]
    raw = checked_slice(data, offset, 12, "FlexSkeletonHeader")
    table_pointer, limb_count, dlist_count = struct.unpack(">IB3xB3x", raw)
    require(limb_count == 21, f"Expected original Link's 21 limbs, got {limb_count}")
    require(dlist_count <= limb_count, "Flex skeleton matrix count exceeds limb count")
    table_offset = segment_offset(table_pointer, 6, len(data), limb_count * 4)
    limb_symbols = {s["offset"]: s for s in symbols if s["kind"] == "Limb"}
    limbs = []
    for index in range(limb_count):
        pointer = struct.unpack_from(">I", data, table_offset + 4 * index)[0]
        limb_offset = segment_offset(pointer, 6, len(data), 16)
        x, y, z, child, sibling, near, far = struct.unpack_from(">hhhBBII", data, limb_offset)
        for edge in (child, sibling):
            require(edge == 255 or edge < limb_count, "Limb link exceeds skeleton")
        for dl in (near, far):
            if dl:
                segment_offset(dl, 6, len(data), 8)
        limbs.append({"index": index, "player_limb": index + 1,
                      "name": limb_symbols.get(limb_offset, {}).get("name", f"limb_{index}"),
                      "offset": limb_offset, "joint_pos": [x, y, z],
                      "child": None if child == 255 else child,
                      "sibling": None if sibling == 255 else sibling,
                      "near_dlist": near, "far_dlist": far,
                      "matrix_index": None, "matrix_index_far": None})
    visited, matrix_counts = set(), [0, 0]

    def visit(index):
        require(index not in visited, "Cycle or multiply parented limb in skeleton")
        visited.add(index)
        limb = limbs[index]
        for lod, (dl_key, out_key) in enumerate((("near_dlist", "matrix_index"), ("far_dlist", "matrix_index_far"))):
            if limb[dl_key]:
                limb[out_key] = matrix_counts[lod]
                matrix_counts[lod] += 1
        if limb["child"] is not None:
            visit(limb["child"])
        if limb["sibling"] is not None:
            visit(limb["sibling"])
    visit(0)
    require(len(visited) == limb_count, "Unreachable limbs in skeleton")
    require(all(count == dlist_count for count in matrix_counts), "Matrix palette count disagrees with skeleton header")
    return {"header_offset": offset, "limb_count": limb_count, "dlist_count": dlist_count,
            "limb_table_offset": table_offset, "limbs": limbs}


def decode_animation(data, animation):
    count, offset = animation["frame_count"], animation["offset"]
    require(0 < count <= 4096, "Unsupported animation frame count")
    raw = checked_slice(data, offset, count * FRAME_STRIDE, animation["name"])
    frames = []
    for frame in range(count):
        values = struct.unpack_from(">66hH", raw, frame * FRAME_STRIDE)
        vectors = [list(values[n:n + 3]) for n in range(0, 66, 3)]
        face = values[-1]
        frames.append({"root_translation": vectors[0], "rotations": vectors[1:],
                       "face_word": face, "eye_index": (face & 15) - 1,
                       "mouth_index": ((face if face < 0x8000 else face - 0x10000) >> 4) - 1})
    return {"name": animation["name"], "frame_count": count, "frame_stride": FRAME_STRIDE,
            "rotation_unit": "signed binary angle: 32768 = 180 degrees",
            "root_unit": "original OoT model unit; scale chosen by native adapter", "frames": frames}


def rgba5551(value):
    # Expand 5-bit components using bit replication, preserving both endpoints.
    return tuple(((((value >> shift) & 31) << 3) | (((value >> shift) & 31) >> 2)) for shift in (11, 6, 1)) + ((value & 1) * 255,)


def decode_texture(data, spec):
    width, height, fmt = spec["width"], spec["height"], spec["format"]
    require(0 < width <= 4096 and 0 < height <= 4096 and width * height <= 1048576,
            "Texture dimensions exceed limits")
    bpp = {"rgba16": 16, "rgba32": 32, "ci8": 8, "ci4": 4, "ia16": 16,
           "ia8": 8, "ia4": 4, "i8": 8, "i4": 4}.get(fmt)
    require(bpp is not None, f"Unsupported texture format {fmt}")
    raw = checked_slice(data, spec["offset"], (width * height * bpp + 7) // 8, "texture")
    pixels = bytearray()
    for index in range(width * height):
        if bpp == 4:
            value = (raw[index // 2] >> (4 if index % 2 == 0 else 0)) & 15
        elif bpp == 8:
            value = raw[index]
        elif bpp == 16:
            value = struct.unpack_from(">H", raw, index * 2)[0]
        else:
            value = struct.unpack_from(">I", raw, index * 4)[0]
        if fmt == "rgba16":
            pixel = rgba5551(value)
        elif fmt == "rgba32":
            pixel = (value >> 24, value >> 16 & 255, value >> 8 & 255, value & 255)
        elif fmt.startswith("ci"):
            require("tlut_offset" in spec, "CI texture has no known palette")
            color = struct.unpack(">H", checked_slice(data, spec["tlut_offset"] + 2 * value, 2, "palette"))[0]
            pixel = rgba5551(color)
        elif fmt == "ia16":
            pixel = (value >> 8,) * 3 + (value & 255,)
        elif fmt == "ia8":
            pixel = ((value >> 4) * 17,) * 3 + ((value & 15) * 17,)
        elif fmt == "ia4":
            pixel = ((value >> 1) * 255 // 7,) * 3 + ((value & 1) * 255,)
        else:
            intensity = value * (17 if bpp == 4 else 1)
            # N64 intensity has the same value in R/G/B/A.
            pixel = (intensity,) * 4
        pixels.extend(pixel)
    return bytes(pixels)


def png_rgba(width, height, pixels):
    require(len(pixels) == width * height * 4, "PNG pixel count mismatch")
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    scanlines = b"".join(b"\x00" + pixels[row * width * 4:(row + 1) * width * 4] for row in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def extract(rom_path, output, include_child=False, animation_names=(), textures=True):
    require(rom_path.is_file(), "ROM input must be an existing local file")
    require(rom_path.stat().st_size <= MAX_ROM_SIZE, "ROM exceeds 64 MiB input limit")
    rom, identity = identify_rom(rom_path.read_bytes())
    metadata = json.loads((HERE / METADATA_FILES[identity["profile"]]).read_text())
    require(metadata["version"] == identity["profile"], "Metadata profile does not match verified ROM")
    require(metadata["source_revision"] == SOURCE_REVISION, "Metadata source revision mismatch")
    dma_offset = metadata["dma_table_offset"]
    dma = read_dma_table(rom, offset=dma_offset)
    require(len(dma) == metadata["dma_entry_count"], "DMA entry count differs from pinned version map")
    names = ["object_link_boy", "gameplay_keep", "link_animetion"]
    if include_child:
        names.append("object_link_child")
    all_anims = {a["name"]: a for a in metadata["animations"]}
    selected = list(animation_names) if animation_names else metadata["default_animations"]
    unknown = sorted(set(selected) - set(all_anims))
    require(not unknown, f"Unknown animation names: {unknown}")
    output = output.resolve()
    require(not output.exists(), "Output already exists; choose a fresh local directory")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".oot-assets-", dir=output.parent))
    try:
        (stage / "segments").mkdir()
        manifest = {"schema_version": 1, "source_revision": SOURCE_REVISION,
                    "rom": identity, "dma_table_offset": dma_offset, "segments": {},
                    "animations": [], "copyright_notice": "Locally extracted user-provided game data. Do not commit or redistribute."}
        blobs = {}
        for name in names:
            spec = metadata["segments"][name]
            index = spec["dma_index"]
            require(index < len(dma), f"Missing DMA entry {index}")
            entry = dma[index]
            data = entry.extract(rom)
            blobs[name] = data
            path = f"segments/{name}.bin"
            (stage / path).write_bytes(data)
            item = {**spec, "file": path, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                    "dma": dataclasses.asdict(entry)}
            if name.startswith("object_link_"):
                item["skeleton"] = decode_skeleton(data, spec)
            if textures:
                for texture in item["symbols"]:
                    if texture["kind"] == "Texture":
                        pixels = decode_texture(data, texture)
                        stem = f"textures/{name}/{texture['name']}"
                        texture["png_file"], texture["rgba_file"] = stem + ".png", stem + ".rgba"
                        destination = stage / texture["png_file"]
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        destination.write_bytes(png_rgba(texture["width"], texture["height"], pixels))
                        (stage / texture["rgba_file"]).write_bytes(pixels)
            manifest["segments"][name] = item
        # Cross-check actual 8-byte animation headers against the pinned metadata.
        keep = blobs["gameplay_keep"]
        for name in selected:
            spec = all_anims[name]
            if "header_offset" in spec:
                raw = checked_slice(keep, spec["header_offset"], 8, "LinkAnimationHeader")
                count, pointer = struct.unpack(">h2xI", raw)
                require(count == spec["frame_count"], f"Animation frame mismatch: {name}")
                require(pointer == 0x07000000 + spec["offset"], f"Animation pointer mismatch: {name}")
            decoded = decode_animation(blobs["link_animetion"], spec)
            path = f"animations/{name}.json"
            write_json(stage / path, decoded)
            raw_path = f"animations/{name}.frames.bin"
            (stage / raw_path).write_bytes(checked_slice(blobs["link_animetion"], spec["offset"],
                                                      spec["frame_count"] * FRAME_STRIDE, name))
            manifest["animations"].append({**spec, "file": path, "raw_file": raw_path, "frame_stride": FRAME_STRIDE})
        write_json(stage / "manifest.json", manifest)
        (stage / ".gitignore").write_text("*\n", encoding="utf-8")
        require(not output.exists(), "Output appeared during extraction; refusing overwrite")
        stage.rename(output)
        return manifest
    except BaseException:
        shutil.rmtree(stage, ignore_errors=True)
        raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, nargs="?", help="Local user-supplied US OoT 1.0 or 1.2 ROM")
    parser.add_argument("--output", type=Path, default=HERE / ".local" / "extracted")
    parser.add_argument("--include-child", action="store_true")
    parser.add_argument("--animation", action="append", default=[], help="Exact data symbol; repeat for a subset")
    parser.add_argument("--no-textures", action="store_true")
    parser.add_argument("--list-animations", action="store_true")
    parser.add_argument("--list-version", choices=("ntsc-1.0-US", "ntsc-1.2-US"), default="ntsc-1.0-US",
                        help="Catalog to list only; extraction always identifies the ROM by exact hash")
    args = parser.parse_args(argv)
    try:
        if args.list_animations:
            metadata = json.loads((HERE / METADATA_FILES[args.list_version]).read_text())
            for spec in metadata["animations"]:
                print(f"{spec['name']}\t{spec['frame_count']}\t0x{spec['offset']:06X}")
            return 0
        require(args.rom is not None, "Supply a local ROM path or --list-animations")
        manifest = extract(args.rom, args.output, args.include_child, args.animation, not args.no_textures)
        print(f"Verified {manifest['rom']['version']}; wrote {len(manifest['segments'])} segments and "
              f"{len(manifest['animations'])} decoded animation tracks to {args.output.resolve()}")
        return 0
    except (AssetError, OSError, json.JSONDecodeError) as error:
        print(f"Extraction stopped: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
