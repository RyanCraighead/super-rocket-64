#!/usr/bin/env python3
"""Convert lawful local OoT Link object segments to bounded skinned meshes.

Standalone Python standard library only. No ROM acquisition, guessed assets,
third-party extractor invocation, or native code execution. See README.md.
"""

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys

try:
    from .pose import palette_mapping, skeleton_order, PoseError
except ImportError:
    from pose import palette_mapping, skeleton_order, PoseError

PIN = "52a510f379afd143aaa0375be9f1e190369572e1"
SCHEMA = "oot-link-mesh-v1"
MAX_SEGMENT = 16 * 1024 * 1024
MAX_COMMANDS = 250000
MAX_VERTICES = 200000
MAX_TRIANGLES = 200000
MAX_MATERIALS = 1024
MAX_TEXTURE_PIXELS = 1024 * 1024
MAX_TEXTURE_BYTES = 64 * 1024 * 1024
MAX_JSON_BYTES = 32 * 1024 * 1024
G_LIGHTING = 0x20000


class DecodeError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise DecodeError(message)


def signed16(value):
    return value - 65536 if value & 0x8000 else value


def rgba32(word):
    return list(word.to_bytes(4, "big"))


def rgba16(value):
    # Exact 5-to-8 bit replication used by conventional N64 texture decoders.
    components = [(value >> s) & 31 for s in (11, 6, 1)]
    return [(v << 3) | (v >> 2) for v in components] + [255 if value & 1 else 0]


class Segments:
    def __init__(self, segments, aliases=None):
        self.segments = dict(segments)
        self.aliases = dict(aliases or {})
        for segment, data in self.segments.items():
            require(type(segment) is int and 0 <= segment < 16, "invalid segment id")
            require(isinstance(data, bytes) and len(data) <= MAX_SEGMENT, "invalid or oversized segment")

    def resolve(self, address):
        require(type(address) is int and 0 <= address <= 0xFFFFFFFF, "invalid segmented address")
        for _ in range(16):
            segment, offset = address >> 24, address & 0xFFFFFF
            require(segment < 16, f"not a segmented address: {address:08x}")
            if segment not in self.aliases:
                require(segment in self.segments, f"unbound segment {segment:02x} at {address:08x}")
                return address
            target = self.aliases[segment]
            require((target & 0xFFFFFF) + offset <= 0xFFFFFF, "segment alias offset overflow")
            address = target + offset
        raise DecodeError("cyclic segment aliases")

    def read(self, address, size, alignment=1):
        require(type(size) is int and 0 <= size <= MAX_SEGMENT, "invalid read size")
        address = self.resolve(address)
        segment, offset = address >> 24, address & 0xFFFFFF
        require(offset % alignment == 0, f"misaligned address {address:08x}")
        data = self.segments[segment]
        require(offset <= len(data) and size <= len(data) - offset,
                f"segment read out of bounds: {address:08x}+{size}")
        return data[offset:offset + size]


def decode_texture(memory, address, fmt, size, width, height, tlut_address=None, palette=0, tlut_type=2):
    """Decode linear texture source bytes, prior to TMEM transfer/swizzling."""
    require(type(width) is int and type(height) is int and 0 < width <= 4096 and 0 < height <= 4096,
            "invalid texture dimensions")
    pixels = width * height
    require(pixels <= MAX_TEXTURE_PIXELS and size in range(4), "texture exceeds decode limit")
    require((fmt, size) in {(0, 2), (0, 3), (2, 0), (2, 1), (3, 0), (3, 1), (3, 2), (4, 0), (4, 1)},
            f"unsupported N64 texture format/size {fmt}/{size}")
    bits = 4 << size
    raw = memory.read(address, (pixels * bits + 7) // 8)
    palette_bytes = None
    if fmt == 2:
        require(tlut_address is not None, "CI texture has no loaded palette")
        require(tlut_type in (2, 3), "CI texture requires RGBA16 or IA16 TLUT mode")
        require(0 <= palette <= 15, "invalid CI4 palette")
        palette_bytes = memory.read(tlut_address + (palette * 32 if size == 0 else 0), (16 if size == 0 else 256) * 2)
    out = bytearray()
    for i in range(pixels):
        if size == 0:
            value = (raw[i // 2] >> (4 if i % 2 == 0 else 0)) & 15
        else:
            start = i * (bits // 8)
            value = int.from_bytes(raw[start:start + bits // 8], "big")
        if fmt == 0:
            color = rgba16(value) if size == 2 else rgba32(value)
        elif fmt == 2:
            entry = int.from_bytes(palette_bytes[value * 2:value * 2 + 2], "big")
            color = rgba16(entry) if tlut_type == 2 else [entry >> 8] * 3 + [entry & 255]
        elif fmt == 3:
            if size == 0:
                intensity = (value >> 1) * 255 // 7
                color = [intensity] * 3 + [255 if value & 1 else 0]
            elif size == 1:
                color = [(value >> 4) * 17] * 3 + [(value & 15) * 17]
            else:
                color = [value >> 8] * 3 + [value & 255]
        else:
            intensity = value * 17 if size == 0 else value
            color = [intensity] * 4
        out.extend(color)
    return bytes(out)


def empty_tile():
    return {"fmt": 0, "size": 2, "line": 0, "tmem": 0, "palette": 0,
            "cm_s": 0, "cm_t": 0, "mask_s": 0, "mask_t": 0,
            "shift_s": 0, "shift_t": 0, "uls": 0, "ult": 0, "lrs": 0, "lrt": 0}


class Decoder:
    """Bounded F3DEX2 interpreter preserving RSP matrix-at-vertex-load semantics.

    This deliberately fails on unsupported transform/control-flow commands.
    It never turns an unknown opcode into a guessed mesh or a silent no-op.
    """
    def __init__(self, memory, matrix_count, texture_symbols=(), max_commands=MAX_COMMANDS):
        require(type(matrix_count) is int and 1 <= matrix_count <= 256, "invalid matrix count")
        self.memory = memory
        self.matrix_count = matrix_count
        self.max_commands = min(max_commands, MAX_COMMANDS)
        self.commands = 0
        self.cache = [None] * 32
        self.vertices, self.triangles, self.materials = [], [], []
        self.vertex_ids, self.material_ids = {}, {}
        self.texture_payloads = {}
        self.texture_bytes = 0
        self.texture_symbols = {(s.get("segment_id", 6) << 24) | s["offset"]: s
                                for s in texture_symbols if "width" in s and "height" in s}
        self.matrix = 0
        self.matrix_stack = []
        # Caller setup is fixed, explicit and recorded, corresponding to opaque Link drawing.
        self.geometry = 0x00220405  # ZBUFFER | SHADE | CULL_BACK | LIGHTING | SHADING_SMOOTH
        self.combine = [0, 0]
        self.prim_color = [255] * 4
        self.env_color = [30, 105, 27, 0]  # original Kokiri tunic
        self.texture_enabled = False
        self.texture_scale = [65535, 65535]
        self.texture_tile = 0
        self.tiles = [empty_tile() for _ in range(8)]
        self.tmem = {}
        self.tlut = None
        self.image = None
        self.othermode_h = 0
        self.othermode_l = 0
        self.warnings = set()
        self.opcodes = {}
        self.root_label = None

    def setup_link_gameplay(self):
        """Original SETUPDL_25 state followed by Player_Draw's cull clear.

        Room-specific dither and environment fog/lights are scene inputs, not
        encoded in the static mesh. White primitive tint is the neutral caller
        input; body display lists remain free to override it.
        """
        self.geometry = 0x00230005
        self.texture_enabled = True
        self.texture_scale = [65535, 65535]
        self.combine = [0x127E03, 0xFF0FF3FF]
        self.othermode_h = 0x00182C10
        self.othermode_l = 0xC8112078

    def vertex(self, value):
        key = tuple(value["position"] + value["uv"] + value["color_normal"] +
                    [value["matrix_index"], value["lit"]] + value["uv_scale"] + [value["uv_processed"]])
        if key not in self.vertex_ids:
            require(len(self.vertices) < MAX_VERTICES, "vertex limit exceeded")
            self.vertex_ids[key] = len(self.vertices)
            self.vertices.append(value)
        return self.vertex_ids[key]

    def texture(self, tile):
        if not self.texture_enabled:
            return None
        source = self.tmem.get(tile["tmem"])
        require(source is not None, "textured primitive uses uninitialized TMEM")
        require(source["kind"] == "block", "partial/tiled TMEM uploads are not yet supported")
        address = source["image"]["address"]
        symbol = self.texture_symbols.get(address)
        width = (tile["lrs"] - tile["uls"]) // 4 + 1
        height = (tile["lrt"] - tile["ult"]) // 4 + 1
        tile_width, tile_height = width, height
        require(tile["uls"] == 0 and tile["ult"] == 0, "nonzero tile origin needs explicit source cropping")
        if symbol:
            # XML dimensions describe the source bytes; a narrower render tile may wrap them.
            width, height = symbol["width"], symbol["height"]
        bits = 4 << tile["size"]
        if symbol and width * height * bits > source["loaded_bytes"] * 8:
            # Original child far-LOD feet reinterpret a 256-byte prefix of the
            # named 32x32 CI8 lower-boot texture as a contiguous 16x16 texture.
            # This is NOT a 16x16 crop with the XML's 32-pixel source stride.
            # Accept only a complete tightly packed block with matching RDP
            # render-tile line stride; reject other partial transfers.
            require(tile_width * tile_height * bits == source["loaded_bytes"] * 8 and
                    tile_width * bits == tile["line"] * 64,
                    "partial texture prefix does not match render tile dimensions/stride")
            width, height = tile_width, tile_height
            self.warnings.add(f"Original DL reinterprets {source['loaded_bytes']} source bytes of "
                              f"{symbol['name']} as {width}x{height} (catalog {symbol['width']}x{symbol['height']})")
        require(width * height * bits <= source["loaded_bytes"] * 8,
                "texture dimensions exceed the bytes loaded into TMEM")
        tlut_address = None
        palette = tile["palette"]
        tlut_type = (self.othermode_h >> 14) & 3
        if tile["fmt"] == 2:
            require(self.tlut is not None, "CI texture has no TLUT load")
            # TLUT source may begin at palette bank N; normalize to bank zero.
            bank = (self.tlut["tmem"] - 256) // 16
            tlut_address = self.tlut["address"] - bank * 32
            needed = (palette + 1) * 16 if tile["size"] == 0 else 256
            require(needed <= bank * 16 + self.tlut["count"], "palette load is too short")
        key = (address, tile["fmt"], tile["size"], width, height, tlut_address, palette, tlut_type)
        name = "tex_" + hashlib.sha256(repr(key).encode()).hexdigest()[:20] + ".rgba"
        if name not in self.texture_payloads:
            require(self.texture_bytes + width * height * 4 <= MAX_TEXTURE_BYTES, "total texture byte budget exceeded")
            payload = decode_texture(self.memory, address, tile["fmt"], tile["size"], width, height,
                                     tlut_address, palette, tlut_type)
            self.texture_payloads[name] = payload
            self.texture_bytes += len(payload)
        out = {"rgba_file": "textures/" + name, "width": width, "height": height,
               "address": address, "tlut_address": tlut_address,
               "binding": source["image"]["binding"], "format": tile["fmt"], "size": tile["size"]}
        if symbol:
            out["symbol"] = symbol["name"]
            out["source_width"], out["source_height"] = symbol["width"], symbol["height"]
        return out

    def material(self):
        tile = copy.deepcopy(self.tiles[self.texture_tile])
        value = {"geometry_mode": self.geometry, "combine": self.combine[:],
                 "prim_color": self.prim_color[:], "env_color": self.env_color[:],
                 "texture_enabled": self.texture_enabled, "texture_scale": self.texture_scale[:],
                 "tile": tile, "texture": self.texture(tile),
                 "other_mode_h": self.othermode_h, "other_mode_l": self.othermode_l}
        key = json.dumps(value, sort_keys=True, separators=(",", ":"))
        if key not in self.material_ids:
            require(len(self.materials) < MAX_MATERIALS, "material limit exceeded")
            self.material_ids[key] = len(self.materials)
            self.materials.append(value)
        return self.material_ids[key]

    def triangle(self, word):
        raw = [(word >> shift) & 255 for shift in (16, 8, 0)]
        require(all(v % 2 == 0 and v < 64 for v in raw), "invalid triangle vertex slot")
        vertices = [self.cache[v // 2] for v in raw]
        require(all(v is not None for v in vertices), "triangle reads an unloaded vertex")
        require(len(self.triangles) < MAX_TRIANGLES, "triangle limit exceeded")
        self.triangles.append({"indices": [self.vertex(v) for v in vertices],
                               "material": self.material(), "root": self.root_label})

    def run(self, address, initial_matrix, label=None):
        require(type(initial_matrix) is int and 0 <= initial_matrix < self.matrix_count, "invalid initial matrix")
        require(not self.matrix_stack, "matrix stack leaked between limb lists")
        self.matrix, self.root_label = initial_matrix, label
        self._list(address, [])
        require(not self.matrix_stack, "unbalanced matrix stack in limb list")

    def _list(self, address, active):
        require(len(active) < 64, "display-list call depth exceeded")
        address = self.memory.resolve(address)
        require(address not in active, "recursive display-list cycle")
        active = active + [address]
        while True:
            require(self.commands < self.max_commands, "display-list command budget exceeded")
            w0, w1 = struct.unpack(">II", self.memory.read(address, 8, 8))
            command_address = address
            address += 8
            self.commands += 1
            op = w0 >> 24
            self.opcodes[f"{op:02x}"] = self.opcodes.get(f"{op:02x}", 0) + 1
            if op == 0xDF:
                return
            if op == 0xDE:
                mode = (w0 >> 16) & 255
                require(mode in (0, 1), "invalid display-list branch mode")
                self._list(w1, active)
                if mode:
                    return
            elif op == 0x01:
                count = (w0 >> 12) & 255
                end = (w0 >> 1) & 127
                first = end - count
                require(1 <= count <= 32 and 0 <= first < end <= 32, "invalid vertex load range")
                raw = self.memory.read(w1, count * 16, 8)
                for i in range(count):
                    x, y, z, _flag, u, v, r, g, b, a = struct.unpack_from(">hhhHhhBBBB", raw, i * 16)
                    self.cache[first + i] = {"position": [x, y, z], "uv": [u, v],
                                             "color_normal": [r, g, b, a],
                                             "matrix_index": self.matrix, "lit": bool(self.geometry & G_LIGHTING),
                                             "uv_scale": self.texture_scale[:], "uv_processed": False}
            elif op == 0x02:
                where, raw_slot = (w0 >> 16) & 255, w0 & 0xFFFF
                require(raw_slot % 2 == 0 and raw_slot < 64, "invalid modified vertex slot")
                slot = raw_slot // 2
                require(self.cache[slot] is not None, "modify reads an unloaded vertex")
                v = copy.deepcopy(self.cache[slot])
                if where == 0x10:
                    v["color_normal"], v["lit"] = rgba32(w1), False
                elif where == 0x14:
                    v["uv"] = [signed16(w1 >> 16), signed16(w1 & 65535)]
                    v["uv_processed"] = True
                else:
                    raise DecodeError("screen-space vertex modification cannot be represented by a skinned mesh")
                self.cache[slot] = v
            elif op in (0x05, 0x06, 0x07):
                self.triangle(w0)
                if op != 0x05:
                    self.triangle(w1)
            elif op == 0xDA:
                params = (w0 & 255) ^ 1
                require(params & 2 and not params & 4 and not params & ~7,
                        "only modelview palette matrix LOAD is supported")
                require(w1 >> 24 == 0x0D and w1 % 64 == 0, "matrix must address 0D palette on a 64-byte boundary")
                matrix = (w1 & 0xFFFFFF) // 64
                require(matrix < self.matrix_count, "matrix palette index out of bounds")
                if params & 1:
                    require(len(self.matrix_stack) < 32, "matrix stack limit exceeded")
                    self.matrix_stack.append(self.matrix)
                self.matrix = matrix
            elif op == 0xD8:
                require(w1 % 64 == 0 and 0 < w1 // 64 <= len(self.matrix_stack), "matrix stack underflow")
                for _ in range(w1 // 64):
                    self.matrix = self.matrix_stack.pop()
            elif op == 0xD9:
                self.geometry = (self.geometry & (w0 & 0xFFFFFF)) | w1
            elif op == 0xD7:
                self.texture_enabled = bool((w0 >> 1) & 127)
                self.texture_tile = (w0 >> 8) & 7
                self.texture_scale = [w1 >> 16, w1 & 65535]
            elif op == 0xFD:
                binding = {8: "eyes", 9: "mouth"}.get(w1 >> 24)
                self.image = {"address": self.memory.resolve(w1), "binding": binding,
                              "fmt": (w0 >> 21) & 7, "size": (w0 >> 19) & 3, "width": (w0 & 4095) + 1}
            elif op == 0xF5:
                tile = self.tiles[(w1 >> 24) & 7]
                tile.update(fmt=(w0 >> 21) & 7, size=(w0 >> 19) & 3, line=(w0 >> 9) & 511,
                            tmem=w0 & 511, palette=(w1 >> 20) & 15, cm_t=(w1 >> 18) & 3,
                            mask_t=(w1 >> 14) & 15, shift_t=(w1 >> 10) & 15,
                            cm_s=(w1 >> 8) & 3, mask_s=(w1 >> 4) & 15, shift_s=w1 & 15)
            elif op == 0xF2:
                self.tiles[(w1 >> 24) & 7].update(uls=(w0 >> 12) & 4095, ult=w0 & 4095,
                                                lrs=(w1 >> 12) & 4095, lrt=w1 & 4095)
            elif op in (0xF3, 0xF4):
                require(self.image is not None, "texture upload without image source")
                tile = self.tiles[(w1 >> 24) & 7]
                require((w0 & 0xFFFFFF) == 0, "partial texture source offset unsupported")
                texels = ((w1 >> 12) & 4095) + 1
                loaded_bytes = (texels * (4 << tile["size"]) + 7) // 8
                self.tmem[tile["tmem"]] = {"image": self.image.copy(), "kind": "block" if op == 0xF3 else "tile",
                                             "loaded_bytes": loaded_bytes}
            elif op == 0xF0:
                require(self.image is not None, "TLUT upload without image source")
                tile = self.tiles[(w1 >> 24) & 7]
                require(tile["tmem"] >= 256 and (tile["tmem"] - 256) % 16 == 0, "invalid TLUT TMEM destination")
                self.tlut = {"address": self.image["address"], "tmem": tile["tmem"], "count": ((w1 >> 14) & 1023) + 1}
            elif op == 0xFC:
                self.combine = [w0 & 0xFFFFFF, w1]
            elif op == 0xFA:
                self.prim_color = rgba32(w1)
            elif op == 0xFB:
                self.env_color = rgba32(w1)
            elif op in (0xE2, 0xE3):
                length = (w0 & 255) + 1
                shift = 32 - ((w0 >> 8) & 255) - length
                require(1 <= length <= 32 and 0 <= shift <= 31, "invalid othermode bit range")
                mask = ((1 << length) - 1) << shift
                if op == 0xE3:
                    self.othermode_h = (self.othermode_h & ~mask) | (w1 & mask)
                else:
                    self.othermode_l = (self.othermode_l & ~mask) | (w1 & mask)
            elif op == 0xEF:
                self.othermode_h, self.othermode_l = w0 & 0xFFFFFF, w1
            elif op == 0x03:
                # A static export retains all geometry; view-dependent rejection belongs to the renderer.
                self.warnings.add("View-dependent G_CULLDL rejection omitted; all original triangles retained")
            elif op in (0x00, 0xE0, 0xE6, 0xE7, 0xE8, 0xE9):
                pass  # NOOP and synchronization carry no mesh/material data.
            else:
                raise DecodeError(f"unsupported F3DEX2 opcode {op:02x} at {command_address:08x}")


def safe_input_file(root, relative):
    require(isinstance(relative, str) and bool(relative), "missing segment path")
    path = (root / relative).resolve()
    require(not Path(relative).is_absolute() and root.resolve() in path.parents, "segment path escapes manifest directory")
    require(path.is_file() and path.stat().st_size <= MAX_SEGMENT, "segment file missing or oversized")
    return path


def pick_roots(segment, age, lod, equipment):
    symbols = {s["name"]: s for s in segment["symbols"]}
    skeleton = segment["skeleton"]
    mapping = palette_mapping(skeleton, lod)
    suffix = "NearDL" if lod == "near" else "FarDL"
    prefix = "gLinkAdult" if age == "adult" else "gLinkChild"
    if equipment == "sword_shield":
        names = {16: prefix + ("LeftHandHoldingMasterSword" if age == "adult" else "LeftFistAndKokiriSword") + suffix,
                 19: prefix + ("RightHandHoldingHylianShield" if age == "adult" else "RightFistAndDekuShield") + suffix,
                 20: prefix + "Sheath" + suffix, 2: prefix + "Waist" + suffix}
    else:
        names = {16: prefix + "LeftHand" + suffix, 19: prefix + "RightHand" + suffix,
                 20: prefix + ("HylianShieldSwordAndSheath" if age == "adult" else "DekuShieldSwordAndSheath") + suffix,
                 2: prefix + "Waist" + suffix}
    roots = []
    for index, _parent in skeleton_order(skeleton):
        limb = skeleton["limbs"][index]
        address = limb.get(lod + "_dlist")
        name = limb.get("name", f"limb_{index}")
        if limb["player_limb"] in names:
            name = names[limb["player_limb"]]
            require(name in symbols, f"required original model symbol missing: {name}")
            require(index in mapping, "limb override would change matrix palette layout")
            address = 0x06000000 | symbols[name]["offset"]
        if address:
            roots.append({"limb": index, "player_limb": limb["player_limb"], "matrix_index": mapping[index],
                          "name": name, "address": address})
    return roots


def convert(manifest_path, output, age="adult", lod="near", equipment="sword_shield"):
    require(age in ("adult", "child") and lod in ("near", "far") and equipment in ("sword_shield", "sheathed"),
            "invalid model selection")
    manifest_path, output = Path(manifest_path), Path(output)
    require(manifest_path.stat().st_size <= 32 * 1024 * 1024, "manifest oversized")
    manifest = json.loads(manifest_path.read_text())
    require(manifest.get("schema_version") == 1, "unsupported extraction manifest schema")
    require(manifest.get("source_revision") == PIN, "extraction source revision does not match the pinned source")
    name = "object_link_boy" if age == "adult" else "object_link_child"
    segment = manifest["segments"][name]
    data = safe_input_file(manifest_path.parent, segment["file"]).read_bytes()
    require(len(data) == segment["size"] and hashlib.sha256(data).hexdigest() == segment["sha256"],
            "object segment size/hash mismatch")
    symbols = {s["name"]: s for s in segment["symbols"]}
    prefix = "gLinkAdult" if age == "adult" else "gLinkChild"
    for symbol in (prefix + "EyesOpenTex", prefix + "MouthClosedTex"):
        require(symbol in symbols, f"neutral face symbol missing: {symbol}")
    aliases = {8: 0x06000000 | symbols[prefix + "EyesOpenTex"]["offset"],
               9: 0x06000000 | symbols[prefix + "MouthClosedTex"]["offset"]}
    # Original gCullBackDList is two commands, fixed by z_player_lib.c.
    cull_list = struct.pack(">IIII", 0xD9FFFFFF, 0x00000400, 0xDF000000, 0)
    segment_data = {6: data, 12: cull_list}
    texture_symbols = [dict(s, segment_id=6) for s in segment["symbols"]]
    if "gameplay_keep" in manifest["segments"]:
        keep = manifest["segments"]["gameplay_keep"]
        keep_data = safe_input_file(manifest_path.parent, keep["file"]).read_bytes()
        require(len(keep_data) == keep["size"] and hashlib.sha256(keep_data).hexdigest() == keep["sha256"],
                "gameplay_keep segment size/hash mismatch")
        segment_data[4] = keep_data
        texture_symbols.extend(dict(s, segment_id=4) for s in keep["symbols"])
    memory = Segments(segment_data, aliases)
    skeleton = copy.deepcopy(segment["skeleton"])
    mapping = palette_mapping(skeleton, lod)
    skeleton["matrix_to_limb"] = [index for index, _parent in skeleton_order(skeleton) if index in mapping]
    for limb in skeleton["limbs"]:
        limb["matrix_index"] = mapping.get(limb["index"])
    decoder = Decoder(memory, skeleton["dlist_count"], texture_symbols)
    decoder.setup_link_gameplay()
    roots = pick_roots(segment, age, lod, equipment)
    for root in roots:
        try:
            decoder.run(root["address"], root["matrix_index"], root["name"])
        except DecodeError as error:
            raise DecodeError(f"{root['name']}: {error}") from error
    require(bool(decoder.triangles), "model produced no triangles")
    result = {"schema_version": SCHEMA, "age": age, "lod": lod, "equipment": equipment,
              "source": {"repository": "https://github.com/zeldaret/oot", "commit": PIN,
                         "object": name, "object_sha256": segment["sha256"],
                         "rom": manifest.get("rom", {}), "manifest_schema_version": 1},
              "skeleton": skeleton, "roots": roots, "vertices": decoder.vertices,
              "triangles": decoder.triangles, "materials": decoder.materials,
              "stats": {"commands": decoder.commands, "opcodes": decoder.opcodes,
                        "vertices": len(decoder.vertices), "triangles": len(decoder.triangles),
                        "materials": len(decoder.materials), "textures": len(decoder.texture_payloads)},
              "warnings": sorted(decoder.warnings),
              "pose_contract": {"joint_count": skeleton["limb_count"] + 1, "root_translation_index": 0,
                                "rotation_start_index": 1, "rotation_format": "signed16_binary_angles",
                                "local_matrix_order": "T*Rz*Ry*Rx", "matrix_layout": "row_major_column_vectors",
                                "uv_format": "signed16_s10.5_texels", "vertex_matrix_binding": "matrix_at_G_VTX_load"}}
    encoded = json.dumps(result, indent=2) + "\n"
    require(len(encoded.encode("utf-8")) <= MAX_JSON_BYTES, "output JSON byte budget exceeded")
    output.mkdir(parents=True, exist_ok=True)
    (output / "textures").mkdir(exist_ok=True)
    for filename, payload in decoder.texture_payloads.items():
        (output / "textures" / filename).write_bytes(payload)
    # Publish the descriptor last; failed decoding cannot leave a partial ready mesh.
    path = output / "mesh.json"
    tmp = output / "mesh.json.tmp"
    tmp.write_text(encoded)
    tmp.replace(path)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--age", choices=("adult", "child"), default="adult")
    parser.add_argument("--lod", choices=("near", "far"), default="near")
    parser.add_argument("--equipment", choices=("sword_shield", "sheathed"), default="sword_shield")
    args = parser.parse_args(argv)
    try:
        result = convert(args.manifest, args.output, args.age, args.lod, args.equipment)
    except (OSError, ValueError, KeyError, TypeError, RecursionError) as error:
        print(f"Link mesh decode failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"mesh": str(args.output / "mesh.json"), **result["stats"]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
