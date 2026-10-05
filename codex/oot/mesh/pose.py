"""Link flex-skeleton palette construction, independently implemented.

The exact transform order and root-translation convention follow pinned OoT
SkelAnime_DrawFlexLod and Matrix_TranslateRotateZYX; see SOURCES.md. This is a
float reference evaluator, not an emulation of the RSP's 16.16 rounding.
Matrices here are row-major and multiply column vectors.
"""

import math


class PoseError(ValueError):
    pass


def identity():
    return [1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.]


def multiply(a, b):
    return [sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4))
            for r in range(4) for c in range(4)]


def translate_rotate_zyx(position, rotation, sin_binang=None):
    """T * Rz * Ry * Rx, preserving all three original binary-angle axes."""
    if len(position) != 3 or len(rotation) != 3:
        raise PoseError("position and rotation need three components")
    if sin_binang is None:
        sin_binang = lambda a: math.sin((int(a) & 0xFFFF) * math.tau / 65536.)
    s = [sin_binang(v) for v in rotation]
    c = [sin_binang(int(v) + 0x4000) for v in rotation]
    sx, sy, sz = s
    cx, cy, cz = c
    return [cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx, float(position[0]),
            sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx, float(position[1]),
            -sy, cy * sx, cy * cx, float(position[2]),
            0., 0., 0., 1.]


def skeleton_order(skeleton):
    """Validate a connected single-root child/sibling tree and return DFS order."""
    limbs = skeleton.get("limbs", [])
    count = skeleton.get("limb_count")
    if not isinstance(count, int) or not 1 <= count <= 256 or len(limbs) != count:
        raise PoseError("invalid skeleton limb_count")
    for i, limb in enumerate(limbs):
        if limb.get("index") != i:
            raise PoseError("skeleton limbs must be ordered by index")
        for key in ("child", "sibling"):
            index = limb.get(key)
            if index is not None and (type(index) is not int or not 0 <= index < count):
                raise PoseError("invalid skeleton child/sibling index")
        p = limb.get("joint_pos")
        if not isinstance(p, list) or len(p) != 3 or any(type(v) is not int or not -32768 <= v <= 32767 for v in p):
            raise PoseError("invalid skeleton joint_pos")
    if limbs[0].get("sibling") is not None:
        raise PoseError("OoT root must not have a sibling")
    order, seen = [], set()

    def visit(index, parent):
        if index in seen:
            raise PoseError("cyclic or multiply-parented skeleton")
        seen.add(index)
        order.append((index, parent))
        limb = limbs[index]
        if limb.get("child") is not None:
            visit(limb["child"], index)
        if limb.get("sibling") is not None:
            visit(limb["sibling"], parent)

    visit(0, None)
    if len(seen) != count:
        raise PoseError("disconnected skeleton")
    return order


def palette_mapping(skeleton, lod="near"):
    """The palette is DFS display-list order, NOT player limb number minus one."""
    if lod not in ("near", "far"):
        raise PoseError("lod must be near or far")
    mapping = {}
    for index, _parent in skeleton_order(skeleton):
        if skeleton["limbs"][index].get(lod + "_dlist"):
            mapping[index] = len(mapping)
    if len(mapping) != skeleton.get("dlist_count"):
        raise PoseError("LOD display lists do not match dlist_count")
    # The extraction schema's matrix_index is the near-palette index.
    if lod == "near":
        for limb in skeleton["limbs"]:
            if "matrix_index" in limb and limb["matrix_index"] != mapping.get(limb["index"]):
                raise PoseError("manifest matrix_index disagrees with DFS order")
    return mapping


def build_palette(skeleton, joints, lod="near", world=None, sin_binang=None):
    """Consume joints[0] translation and joints[1..limb_count] rotations.

    Does not flatten, zero, rescale, or substitute any original animation axis.
    Extra packed face information may follow the joint list; it is not a limb.
    A world/actor matrix can apply scene placement and model scale externally.
    """
    count = skeleton.get("limb_count", 0)
    if len(joints) < count + 1:
        raise PoseError("pose is missing root translation or limb rotations")
    for joint in joints[:count + 1]:
        if len(joint) != 3 or any(type(v) is not int or not -32768 <= v <= 32767 for v in joint):
            raise PoseError("pose components must be signed 16-bit integers")
    if world is None:
        world = identity()
    if len(world) != 16 or any(not math.isfinite(v) for v in world):
        raise PoseError("invalid world matrix")
    mapping = palette_mapping(skeleton, lod)
    matrices, palette = {}, [None] * len(mapping)
    for index, parent in skeleton_order(skeleton):
        translation = joints[0] if index == 0 else skeleton["limbs"][index]["joint_pos"]
        local = translate_rotate_zyx(translation, joints[index + 1], sin_binang)
        matrix = multiply(world if parent is None else matrices[parent], local)
        matrices[index] = matrix
        if index in mapping:
            palette[mapping[index]] = matrix
    return {"matrices": palette, "limb_matrices": matrices,
            "matrix_to_limb": [i for i, _p in skeleton_order(skeleton) if i in mapping]}


def transform_point(matrix, position):
    return [sum(matrix[row * 4 + col] * position[col] for col in range(3)) + matrix[row * 4 + 3]
            for row in range(3)]


def transform_normal(matrix, normal):
    """Rotation/uniform-scale normal transform, appropriate to Link's bones."""
    v = [sum(matrix[row * 4 + col] * normal[col] for col in range(3)) for row in range(3)]
    length = math.sqrt(sum(x * x for x in v))
    return [x / length for x in v] if length else [0., 0., 0.]
