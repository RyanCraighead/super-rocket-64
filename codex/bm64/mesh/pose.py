"""Bounded original BM64 model hierarchy and pose-set reader.

Format recovered from the user's US 1.0 input, particularly 8022AF1C,
8022CFC8, 8022C2A4 and 8022D414. No game assets are embedded here.
"""
from __future__ import annotations

from dataclasses import dataclass
import math
import struct


class ModelFormatError(ValueError):
    """An unsupported or malformed original-model packet."""


@dataclass(frozen=True)
class Entry:
    index: int
    kind: int
    parameter: int
    offset: int


def entries(data: bytes) -> tuple[Entry, ...]:
    if len(data) < 12 or data[:4] != b"64\x008":
        raise ModelFormatError("Missing original model signature")
    count = struct.unpack_from(">I", data, 4)[0]
    if not 1 <= count <= 4096 or 12 + count * 12 > len(data):
        raise ModelFormatError("Invalid model entry table")
    return tuple(Entry(i, *struct.unpack_from(">III", data, 12 + i * 12))
                 for i in range(count))


def section(data: bytes, kind: int, entry_index: int | None = None) -> tuple[Entry, int]:
    table = entries(data)
    matches = [e for e in table if e.kind == kind and
               (entry_index is None or e.index == entry_index)]
    if len(matches) != 1:
        raise ModelFormatError(f"Expected exactly one type-{kind} section")
    e = matches[0]
    if e.offset < 12 + 12 * len(table) or e.offset >= len(data):
        raise ModelFormatError("Section starts outside model payload")
    end = min((x.offset for x in table if e.offset < x.offset <= len(data)),
              default=len(data))
    return e, end


class Reader:
    def __init__(self, data: bytes, start: int, end: int):
        self.data, self.cursor, self.end = data, start, end

    def read(self, fmt: str):
        size = struct.calcsize(">" + fmt)
        if self.cursor < 0 or self.cursor + size > self.end:
            raise ModelFormatError("Truncated original model section")
        values = struct.unpack_from(">" + fmt, self.data, self.cursor)
        self.cursor += size
        return values

    def count(self, limit: int = 4096) -> int:
        value, = self.read("I")
        if not 0 <= value <= limit:
            raise ModelFormatError("Unbounded model count")
        return value

    def finish(self):
        padding = self.data[self.cursor:self.end]
        if len(padding) > 7 or any(padding):
            raise ModelFormatError("Unsupported trailing section data")


@dataclass(frozen=True)
class Transform:
    translation: tuple[float, float, float]
    rotation: tuple[float, float, float]
    scale: tuple[float, float, float]

    @classmethod
    def read(cls, reader: Reader) -> "Transform":
        v = reader.read("9f")
        if not all(math.isfinite(x) for x in v):
            raise ModelFormatError("Non-finite model transform")
        return cls(v[:3], v[3:6], v[6:])

    def values(self) -> tuple[float, ...]:
        return self.translation + self.rotation + self.scale

    def to_dict(self) -> dict:
        return {"translation": list(self.translation), "rotation": list(self.rotation),
                "scale": list(self.scale)}


@dataclass(frozen=True)
class Limb:
    index: int
    parent: int | None
    child: int | None
    sibling: int | None
    display_entry: int
    transform: Transform

    def to_dict(self) -> dict:
        return {"index": self.index, "parent": self.parent, "child": self.child,
                "sibling": self.sibling, "display_entry": self.display_entry,
                **self.transform.to_dict()}


@dataclass(frozen=True)
class Hierarchy:
    root: int
    limbs: tuple[Limb, ...]
    source_entry: int

    def to_dict(self) -> dict:
        return {"root": self.root, "source_entry": self.source_entry,
                "rotation_units": "degrees", "limbs": [x.to_dict() for x in self.limbs]}

    def resolve_path(self, path: tuple[int, ...]) -> int:
        # 8022C2A4 ignores element zero; later elements select child-array indices.
        if not path or path[0] != 0:
            raise ModelFormatError("Unsupported pose root path")
        index = self.root
        for ordinal in path[1:]:
            index = self.limbs[index].child
            for _ in range(ordinal):
                if index is None:
                    raise ModelFormatError("Pose path exceeds child list")
                index = self.limbs[index].sibling
            if index is None:
                raise ModelFormatError("Pose path exceeds child list")
        return index


def parse_hierarchy(data: bytes, entry_index: int | None = None) -> Hierarchy:
    e, end = section(data, 1, entry_index)
    size = end - e.offset
    if size % 48 or not 1 <= size // 48 <= 4096:
        raise ModelFormatError("Hierarchy must contain bounded 48-byte records")
    count = size // 48
    if e.parameter >= count:
        raise ModelFormatError("Hierarchy root outside node array")
    r = Reader(data, e.offset, end)
    raw = []
    for i in range(count):
        display, = r.read("i")
        transform = Transform.read(r)
        sibling, child = r.read("ii")
        links = tuple(i + x if x else None for x in (sibling, child))
        if any(x is not None and not 0 <= x < count for x in links):
            raise ModelFormatError("Hierarchy link outside node array")
        if display < -1 or display >= len(entries(data)):
            raise ModelFormatError("Hierarchy display entry outside model")
        raw.append((display, transform, *links))
    parents = {}
    todo = [(e.parameter, None)]
    while todo:
        i, parent = todo.pop()
        if i in parents:
            raise ModelFormatError("Cyclic or multiply-parented hierarchy")
        parents[i] = parent
        sibling, child = raw[i][2:]
        if sibling is not None:
            todo.append((sibling, parent))
        if child is not None:
            todo.append((child, i))
    if len(parents) != count:
        raise ModelFormatError("Unreachable hierarchy records")
    return Hierarchy(e.parameter,
                     tuple(Limb(i, parents[i], row[3], row[2], row[0], row[1])
                           for i, row in enumerate(raw)), e.index)


@dataclass(frozen=True)
class PoseOverride:
    path: tuple[int, ...]
    limb: int
    transform: Transform


@dataclass(frozen=True)
class PoseSet:
    identifier: int
    overrides: tuple[PoseOverride, ...]


def parse_pose_sets(data: bytes, hierarchy: Hierarchy) -> tuple[PoseSet, ...]:
    if not any(e.kind == 2 for e in entries(data)):
        return ()
    e, end = section(data, 2)
    if e.parameter > 4096:
        raise ModelFormatError("Unbounded pose-set count")
    r = Reader(data, e.offset, end)
    poses = []
    for _ in range(e.parameter):
        identifier, = r.read("I")
        count = r.count()
        overrides = []
        for _ in range(count):
            path_count = r.count(128)
            path = r.read(f"{path_count}I")
            limb = hierarchy.resolve_path(path)
            overrides.append(PoseOverride(path, limb, Transform.read(r)))
        poses.append(PoseSet(identifier, tuple(overrides)))
    r.finish()
    return tuple(poses)


IDENTITY = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)


def multiply(a: tuple[float, ...], b: tuple[float, ...]) -> tuple[float, ...]:
    """Row-major storage, column-vector multiplication."""
    return tuple(sum(a[row * 4 + k] * b[k * 4 + column] for k in range(4))
                 for row in range(4) for column in range(4))


def local_matrix(transform: Transform) -> tuple[float, ...]:
    """Native limb draw: T * Rz * Rx * Ry; limb scale is not consumed.

    8022D280 calls 8029CA80 then 8029C7A8. The latter's explicit nine-value
    matrix (8029C828..C8B8) is Ry*Rx*Rz for N64 row vectors, transposed here.
    8022D980 supplies only translation and rotation, never the scale fields.
    This evaluator uses host sin/cos; it is not a bit-exact RSP emulator.
    """
    x, y, z = (math.radians(v) for v in transform.rotation)
    sx, cx, sy, cy, sz, cz = math.sin(x), math.cos(x), math.sin(y), math.cos(y), math.sin(z), math.cos(z)
    tx, ty, tz = transform.translation
    return (cy * cz - sx * sy * sz, -cx * sz, sx * cy * sz + sy * cz, tx,
            sx * sy * cz + cy * sz, cx * cz, sy * sz - sx * cy * cz, ty,
            -cx * sy, sx, cx * cy, tz,
            0., 0., 0., 1.)


def palette(hierarchy: Hierarchy, transforms: tuple[Transform, ...] | None = None,
            billboard_entries=(), camera_rotation: tuple[float, ...] = IDENTITY
            ) -> tuple[tuple[float, ...], ...]:
    """Model-space matrices, including original camera-facing type-5 limbs.

    camera_rotation is a 4x4 row-major camera-to-model-space pure rotation.
    Supply the inverse rotation of the renderer's combined model/view matrix.
    Default identity is useful for a straight-on offscreen reference. The
    top-level host/object transform and scale belong outside this palette.
    Billboard replacement affects only that limb's display list; descendants
    inherit the original limb matrix, matching 8022D58C's push/draw/pop.
    """
    if transforms is None:
        transforms = tuple(l.transform for l in hierarchy.limbs)
    if len(transforms) != len(hierarchy.limbs):
        raise ModelFormatError("Pose/hierarchy limb count mismatch")
    if (len(camera_rotation) != 16 or not all(math.isfinite(v) for v in camera_rotation) or
            any(abs(camera_rotation[i] - IDENTITY[i]) > 1e-6 for i in (3, 7, 11, 12, 13, 14, 15))):
        raise ModelFormatError("Billboard camera basis must be a pure 4x4 rotation")
    for i in range(3):
        for j in range(3):
            dot = sum(camera_rotation[k * 4 + i] * camera_rotation[k * 4 + j] for k in range(3))
            if abs(dot - (1. if i == j else 0.)) > 1e-5:
                raise ModelFormatError("Billboard camera basis must be orthonormal")
    billboard_entries = set(billboard_entries)
    original = {}
    pending = set(range(len(transforms)))
    while pending:
        ready = [i for i in pending if hierarchy.limbs[i].parent is None or
                 hierarchy.limbs[i].parent in original]
        if not ready:
            raise ModelFormatError("Cyclic hierarchy parent matrix references")
        for i in ready:
            parent = hierarchy.limbs[i].parent
            original[i] = multiply(original[parent] if parent is not None else IDENTITY,
                                   local_matrix(transforms[i]))
            pending.remove(i)
    result = []
    for limb in hierarchy.limbs:
        m = list(original[limb.index])
        if limb.display_entry in billboard_entries:
            for row in range(3):
                for column in range(3):
                    m[row * 4 + column] = camera_rotation[row * 4 + column]
        result.append(tuple(m))
    return tuple(result)
