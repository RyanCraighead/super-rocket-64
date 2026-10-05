"""Rebuild the two source-derived audio buffers for the local engine setup.

This module uses the bundled Python standard library. The sequence prefix and
sample AIFCs are deterministic build artifacts derived from checked-in source
audio inputs; owned US-ROM spans are supplied separately by the setup cache.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import shutil
import sys
import zlib


PREFIX_SIZE = 16_832
PREFIX_SHA256 = "fa027210a5d81a122968a0161005cd9ecd75b35183f08963a22366f11dfd23c8"
ENCODING_MANIFEST = "codex/windows/audio_inputs/encoding-manifest.json"
ENCODED_DIR = "codex/windows/audio_inputs/encoded"
CUSTOM_INPUT_DIR = "codex/windows/ctl_inputs/samples"
CTL_SETUP = "codex/windows/owned_ctl_setup.py"


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _cancel(cancel_check) -> None:
    if cancel_check():
        raise InterruptedError("Audio setup was canceled")


def _apply_overlays(data: bytearray, group: dict, packed: bytes,
                    cancel_check=lambda: None) -> None:
    ranges = group.get("ranges")
    if not isinstance(ranges, list) or not ranges:
        raise ValueError("Audio overlay recipe has no ranges")
    if type(group.get("target_size")) is not int or len(data) != group["target_size"]:
        raise ValueError("Audio target buffer size does not match its recipe")
    if type(group.get("bytes")) is not int or len(packed) != group["bytes"]:
        raise ValueError("Packed owned-audio buffer size does not match its recipe")
    if _sha256(packed) != group.get("sha256"):
        raise ValueError("Packed owned-audio buffer hash does not match its recipe")

    packed_cursor = 0
    destination_end = 0
    for span in ranges:
        _cancel(cancel_check)
        if not isinstance(span, dict):
            raise ValueError("Invalid audio overlay record")
        source = span.get("packed_offset")
        destination = span.get("destination_offset")
        size = span.get("size")
        if any(type(x) is not int or x < 0 for x in (source, destination, size)) or size == 0:
            raise ValueError("Invalid audio overlay bounds")
        if source != packed_cursor or destination < destination_end:
            raise ValueError("Audio overlay records are unordered or overlapping")
        if source + size > len(packed) or destination + size > len(data):
            raise ValueError("Audio overlay exceeds its source or target buffer")
        data[destination:destination + size] = packed[source:source + size]
        packed_cursor += size
        destination_end = destination + size
    if packed_cursor != len(packed):
        raise ValueError("Audio overlays do not consume the packed owned-audio buffer")


def source_music(prefix_path, group: dict, packed: bytes, expected_size: int,
                 expected_sha256: str, cancel_check=lambda: None) -> bytes:
    """Build `sound/sequences.bin` from the audited sequence prefix and ROM spans."""
    _cancel(cancel_check)
    prefix_path = Path(prefix_path)
    prefix = prefix_path.read_bytes()
    if len(prefix) != PREFIX_SIZE or _sha256(prefix) != PREFIX_SHA256:
        raise ValueError("The source-built 00 sound-player prefix is missing or changed")
    if type(expected_size) is not int or expected_size != group.get("target_size"):
        raise ValueError("Music target size does not match the owned-audio recipe")
    if len(prefix) > expected_size:
        raise ValueError("The source-built music prefix exceeds the target buffer")

    output = bytearray(expected_size)
    output[:len(prefix)] = prefix
    first_overlay = min(item.get("destination_offset", -1) for item in group.get("ranges", []))
    if first_overlay != len(prefix):
        raise ValueError("The first owned-ROM sequence does not follow the generated prefix")
    _apply_overlays(output, group, packed, cancel_check)
    result = bytes(output)
    if len(result) != expected_size or _sha256(result) != expected_sha256:
        raise ValueError("Rebuilt sequence data did not match the registered target")
    return result


def _load_encoding_manifest(root: Path):
    manifest_path = root / ENCODING_MANIFEST
    raw = manifest_path.read_bytes()
    if len(raw) > 2 * 1024 * 1024:
        raise ValueError("The encoded-sample manifest is too large")
    manifest = json.loads(raw)
    if not isinstance(manifest, dict) or manifest.get("format") != "sr64-audio-encoding@1":
        raise ValueError("Unsupported encoded-sample manifest")
    samples = manifest.get("samples")
    if not isinstance(samples, list) or len(samples) != 123:
        raise ValueError("The encoded-sample manifest must contain all 123 source samples")
    return manifest_path, manifest, samples


def _sample_paths(root: Path, samples: list[dict]):
    input_root = root / CUSTOM_INPUT_DIR
    encoded_root = root / ENCODED_DIR
    by_source = {}
    for row in samples:
        if not isinstance(row, dict):
            raise ValueError("Invalid encoded-sample manifest row")
        source_name, encoded_name = row.get("source"), row.get("encoded")
        if not isinstance(source_name, str) or not isinstance(encoded_name, str) or not source_name or not encoded_name:
            raise ValueError("Invalid source or encoded sample path")
        if any(mark in source_name or mark in encoded_name for mark in ("\\", ":", "\0")):
            raise ValueError("Encoded-sample manifest contains an unsafe path")
        source_rel = PurePosixPath(source_name)
        encoded_rel = PurePosixPath(encoded_name)
        if source_rel.is_absolute() or encoded_rel.is_absolute() or ".." in source_rel.parts or ".." in encoded_rel.parts:
            raise ValueError("Encoded-sample manifest contains an unsafe path")
        if source_rel.parts[:2] == ("sound", "samples"):
            source_rel = PurePosixPath(*source_rel.parts[2:])
        elif source_rel.parts[:1] == ("samples",):
            source_rel = PurePosixPath(*source_rel.parts[1:])
        else:
            raise ValueError("Encoded-sample source must be under the custom sample inputs")
        if not source_rel.parts or source_rel.suffix.lower() != ".aiff":
            raise ValueError("Encoded-sample source path has an unsupported extension")
        if encoded_rel.parts[0] == "encoded":
            encoded_rel = PurePosixPath(*encoded_rel.parts[1:])
        if not encoded_rel.parts or encoded_rel.suffix.lower() != ".aifc":
            raise ValueError("Encoded sample path has an unsupported extension")
        if source_rel.with_suffix(".aifc").as_posix() != encoded_rel.as_posix():
            raise ValueError("Encoded sample does not correspond to its declared source")
        key = source_rel.as_posix()
        if key in by_source:
            raise ValueError("Duplicate source sample in encoded-sample manifest")
        source_root = input_root.resolve()
        encoded_base = encoded_root.resolve()
        source = source_root.joinpath(*source_rel.parts).resolve()
        encoded = encoded_base.joinpath(*encoded_rel.parts).resolve()
        if not source.is_relative_to(source_root) or not encoded.is_relative_to(encoded_base):
            raise ValueError("Encoded-sample manifest escapes its input directory")
        source_size, source_hash = row.get("source_size"), row.get("source_sha256")
        encoded_size, encoded_hash = row.get("encoded_size"), row.get("encoded_sha256")
        if any(type(x) is not int or x <= 0 for x in (source_size, encoded_size)):
            raise ValueError("Invalid source or encoded sample size")
        if not source.is_file() or source.stat().st_size != source_size or _sha256(source.read_bytes()) != source_hash:
            raise ValueError("A custom source AIFF is missing or changed: " + key)
        if not encoded.is_file() or encoded.stat().st_size != encoded_size or _sha256(encoded.read_bytes()) != encoded_hash:
            raise ValueError("A source-encoded AIFC is missing or changed: " + key)
        by_source[key] = (source, encoded)
    return input_root.resolve(), by_source


def source_samples(root, rom, workspace, group: dict, packed: bytes,
                   expected_size: int, expected_sha256: str,
                   cancel_check=lambda: None) -> tuple[bytes, bytes]:
    """Build exact `sound_data.tbl` and return it with its matching CTL metadata.

    The repository CTL generator is reused for layout and ROM metadata. Its
    custom-AIFF metadata writer is temporarily redirected to the separately
    verified source-encoded AIFCs. The hook is restored even when cancelled.
    """
    root = Path(root).resolve()
    workspace = Path(workspace).resolve()
    _cancel(cancel_check)
    _, _, sample_rows = _load_encoding_manifest(root)
    input_root, encoded_samples = _sample_paths(root, sample_rows)
    if len(encoded_samples) != 123:
        raise ValueError("The encoded-sample manifest has duplicate or missing sources")

    ctl_path = root / CTL_SETUP
    spec = importlib.util.spec_from_file_location("source_audio_owned_ctl", ctl_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Bundled CTL setup source is missing")
    ctl = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = ctl
    spec.loader.exec_module(ctl)

    original_writer = ctl._write_custom_metadata
    copied = set()
    prior_work = {path.resolve() for path in workspace.glob("ctl-*")}

    def write_verified_aifc(source, destination, writer_type, inner_cancel_check):
        _cancel(cancel_check)
        _cancel(inner_cancel_check)
        source = Path(source).resolve()
        try:
            relative = source.relative_to(input_root).as_posix()
        except ValueError as error:
            raise ValueError("CTL generator requested a custom sample outside its inputs") from error
        pair = encoded_samples.get(relative)
        if pair is None:
            raise ValueError("CTL generator requested an unmanifested custom sample: " + relative)
        _, encoded = pair
        destination = Path(destination)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(encoded, destination)
        copied.add(relative)

    try:
        ctl._write_custom_metadata = write_verified_aifc
        ctl_bytes = ctl.generate(root, rom, workspace, cancel_check)
    finally:
        ctl._write_custom_metadata = original_writer

    if copied != set(encoded_samples):
        missing = sorted(set(encoded_samples) - copied)
        raise ValueError("CTL layout did not consume every manifested custom sample: " + ", ".join(missing[:5]))
    work_dirs = [path for path in workspace.glob("ctl-*") if path.resolve() not in prior_work]
    if len(work_dirs) != 1:
        raise RuntimeError("CTL assembler did not leave its transient TBL output")
    compressed_tbl = work_dirs[0] / "sound/sound_data_compressed.tbl"
    if not compressed_tbl.is_file():
        raise RuntimeError("CTL assembler did not produce the expected transient TBL")
    try:
        tbl = bytearray(zlib.decompress(compressed_tbl.read_bytes()))
    except zlib.error as error:
        raise RuntimeError("The transient TBL could not be decompressed") from error
    if len(tbl) != expected_size or expected_size != group.get("target_size"):
        raise ValueError("Generated TBL size does not match the registered target")
    _apply_overlays(tbl, group, packed, cancel_check)
    tbl_bytes = bytes(tbl)
    if _sha256(tbl_bytes) != expected_sha256:
        raise ValueError("Rebuilt sample data did not match the registered target")
    return tbl_bytes, ctl_bytes
