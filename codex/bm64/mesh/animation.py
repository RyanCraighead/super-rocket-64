"""Original BM64 key-pose animation reader and local-transform evaluator.

Implements the type-3 packets consumed by 8022AF1C, the inclusive track
intervals and interpolation in 8022C530, and the additive pose weights in
8022BCC8. Frames are original animation-frame units, not host elapsed seconds.
"""
from __future__ import annotations

from dataclasses import dataclass
import math
import struct

try:
    from .pose import (Hierarchy, ModelFormatError, PoseSet, Reader, Transform,
                       entries, parse_hierarchy, parse_pose_sets, section)
except ImportError:
    from pose import (Hierarchy, ModelFormatError, PoseSet, Reader, Transform,
                      entries, parse_hierarchy, parse_pose_sets, section)


def f32(value: float) -> float:
    return struct.unpack(">f", struct.pack(">f", value))[0]


@dataclass(frozen=True)
class PoseTrack:
    start: int
    poses: tuple[int, ...]
    durations: tuple[int, ...]

    @property
    def end(self):
        return self.start + sum(self.durations)

    def to_dict(self):
        return {"opcode": 47, "start": self.start, "end": self.end,
                "poses": list(self.poses), "durations": list(self.durations)}


@dataclass(frozen=True)
class WeightTrack:
    start: int
    duration: int
    pose: int
    keys: tuple[tuple[int, float], ...]

    @property
    def end(self):
        return self.start + self.duration

    def to_dict(self):
        return {"opcode": 48, "start": self.start, "end": self.end,
                "pose": self.pose, "keys": [list(x) for x in self.keys]}


@dataclass(frozen=True)
class Animation:
    identifier: int
    duration: int
    tracks: tuple[PoseTrack | WeightTrack, ...]

    def to_dict(self):
        return {"id": self.identifier, "duration": self.duration,
                "tracks": [x.to_dict() for x in self.tracks]}


def parse_animations(data: bytes, poses: tuple[PoseSet, ...]) -> tuple[Animation, ...]:
    if not any(e.kind == 3 for e in entries(data)):
        return ()
    e, end = section(data, 3)
    if e.parameter > 4096:
        raise ModelFormatError("Unbounded animation count")
    r = Reader(data, e.offset, end)
    result = []
    for _ in range(e.parameter):
        identifier, = r.read("I")
        count = r.count()
        duration = r.count(1_000_000)
        tracks = []
        for _ in range(count):
            opcode, = r.read("I")
            if opcode == 47:
                key_count = r.count()
                start = r.count(1_000_000)
                pairs = tuple(r.read("II") for _ in range(key_count))
                if key_count < 2 or any(p >= len(poses) for p, _ in pairs):
                    raise ModelFormatError("Invalid animation pose reference")
                spans = tuple(d for _, d in pairs)
                if spans[-1] != 0 or any(d < 1 or d > 1_000_000 for d in spans[:-1]):
                    raise ModelFormatError("Unsupported key-pose durations")
                track = PoseTrack(start, tuple(p for p, _ in pairs), spans)
            elif opcode == 48:
                start = r.count(1_000_000)
                span = r.count(1_000_000)
                key_count = r.count()
                pose = r.count()
                keys = tuple(r.read("If") for _ in range(key_count))
                if (pose >= len(poses) or key_count < 2 or keys[0][0] != 0 or
                        keys[-1][0] != span or
                        any(not math.isfinite(v) for _, v in keys) or
                        any(a[0] >= b[0] for a, b in zip(keys, keys[1:]))):
                    raise ModelFormatError("Invalid weighted-pose track")
                track = WeightTrack(start, span, pose, keys)
            else:
                raise ModelFormatError(f"Unsupported animation opcode {opcode}")
            if track.end > duration:
                raise ModelFormatError("Animation track exceeds declared duration")
            tracks.append(track)
        result.append(Animation(identifier, duration, tuple(tracks)))
    r.finish()
    return tuple(result)


def _lerp(a: float, b: float, weight: float) -> float:
    return f32(a + f32(f32(b - a) * weight))


def _interpolate(a: Transform, b: Transform, weight: float) -> Transform:
    # 8022BF9C normalizes each endpoint above 180 once; it does not apply a
    # shortest-arc correction to their difference.
    av, bv = list(a.values()), list(b.values())
    for i in range(3, 6):
        if av[i] > 180:
            av[i] = f32(av[i] - 360)
        if bv[i] > 180:
            bv[i] = f32(bv[i] - 360)
    v = tuple(_lerp(x, y, weight) for x, y in zip(av, bv))
    return Transform(v[:3], v[3:6], v[6:])


@dataclass(frozen=True)
class AnimationRig:
    hierarchy: Hierarchy
    poses: tuple[PoseSet, ...]
    animations: tuple[Animation, ...]

    @classmethod
    def parse(cls, data: bytes) -> "AnimationRig":
        hierarchy = parse_hierarchy(data)
        poses = parse_pose_sets(data, hierarchy)
        return cls(hierarchy, poses, parse_animations(data, poses))

    def sample(self, animation_index: int, frame: float,
               base: tuple[Transform, ...] | None = None) -> tuple[Transform, ...]:
        if (type(animation_index) is not int or
                not 0 <= animation_index < len(self.animations)):
            raise ModelFormatError("Animation index outside original table")
        animation = self.animations[animation_index]
        if not math.isfinite(frame) or not 0 <= frame <= animation.duration:
            raise ModelFormatError("Frame outside original animation interval")
        frame = f32(frame)
        bind = tuple(limb.transform for limb in self.hierarchy.limbs)
        if base is not None and len(base) != len(bind):
            raise ModelFormatError("Layered pose/hierarchy limb count mismatch")
        output = list(bind if base is None else base)
        for track in animation.tracks:
            if not track.start <= frame <= track.end:
                continue
            local = f32(frame - track.start)
            if isinstance(track, PoseTrack):
                times, time = [], 0
                for duration in track.durations:
                    times.append(time)
                    time += duration
                upper = next((i for i in range(1, len(times)) if int(local) < times[i]),
                             len(times) - 1)
                weight = f32(f32(local - times[upper - 1]) /
                             (times[upper] - times[upper - 1]))
                left = {x.limb: x.transform for x in self.poses[track.poses[upper - 1]].overrides}
                right = {x.limb: x.transform for x in self.poses[track.poses[upper]].overrides}
                for index in left.keys() | right.keys():
                    output[index] = _interpolate(left.get(index, bind[index]),
                                                 right.get(index, bind[index]), weight)
            else:
                # 8022BE48 compares truncated local time to the next key first.
                upper = next(i for i, (time, _) in enumerate(track.keys)
                             if time >= int(local))
                if track.keys[upper][0] == int(local):
                    weight = track.keys[upper][1]
                else:
                    ta, a = track.keys[upper - 1]
                    tb, b = track.keys[upper]
                    slope = f32(f32(b - a) / (tb - ta))
                    weight = f32(a + f32(f32(local - ta) * slope))
                for item in self.poses[track.pose].overrides:
                    v = tuple(f32(cur + f32(f32(target - original) * weight))
                              for cur, target, original in zip(output[item.limb].values(),
                                                              item.transform.values(),
                                                              bind[item.limb].values()))
                    output[item.limb] = Transform(v[:3], v[3:6], v[6:])
        return tuple(output)

    def sample_channels(self, channels) -> tuple[Transform, ...]:
        """Apply active (animation_index, frame) pairs in original channel order.

        8022827C passes zero to C530 for the first active channel, then a
        nonzero count for later channels. The first resets bind transforms;
        subsequent channels preserve prior results except where overridden.
        """
        result = tuple(limb.transform for limb in self.hierarchy.limbs)
        for animation_index, frame in channels:
            result = self.sample(animation_index, frame, base=result)
        return result

    def frame_to_dict(self, animation_index: int, frame: float) -> dict:
        return {"animation": animation_index, "frame": frame,
                "limbs": [{"index": i, **t.to_dict()}
                          for i, t in enumerate(self.sample(animation_index, frame))]}

    def to_dict(self) -> dict:
        return {"skeleton": self.hierarchy.to_dict(),
                "pose_sets": [{"id": p.identifier,
                               "overrides": [{"path": list(o.path), "limb": o.limb,
                                              **o.transform.to_dict()} for o in p.overrides]}
                              for p in self.poses],
                "animations": [a.to_dict() for a in self.animations]}


def advance_channel(animation: Animation, frame: float, flags: int) -> tuple[float, int]:
    """One original 8022827C channel update, after evaluating its current pose.

    Flags 0x40 freezes the clock; 0x20 requests stop-at-end rather than looping.
    This does not infer a wall-clock frequency or the caller's pause policy.
    """
    if animation.duration <= 0 or not math.isfinite(frame) or frame < 0:
        raise ModelFormatError("Invalid animation channel clock")
    frame = f32(frame)
    if flags & 0x40:
        return frame, flags
    frame = f32(frame + 1.)
    if frame >= animation.duration:
        if flags & 0x20:
            frame = f32(animation.duration - f32(.001))
            flags = (flags | 0x40) & ~0x20000
        else:
            frame = f32(frame - animation.duration)
    return frame, flags
