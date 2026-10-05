"""Rebuild the Octane sound control file from bundled metadata and an owned ROM.

Only the returned CTL is intended for use by the caller. All disassembly and
assembly products, including a disposable TBL, are confined to ``workspace``.
The script deliberately does not decode or encode PCM audio and starts no
external process.
"""

from __future__ import annotations

import hashlib
import importlib.util
import io
import json
import os
import pathlib
import shutil
import struct
import sys
import tempfile
import zlib
from collections import defaultdict
from typing import Callable


ROM_SHA1 = "9bef1128717f958171a4afac3ed78ee2bb4e86ce"
CTL_OFFSET = 5_748_512
CTL_SIZE = 97_856
TBL_OFFSET = 5_846_368
TBL_SIZE = 2_216_704
EXPECTED_CTL_SIZE = 193_920
EXPECTED_CTL_SHA256 = "e0d97745faf18d59015fcf22bfe25d092e82d7a2d620c9eaa5753141d180c07d"
EXTENDED_SOURCE_ORDER = ("instruments", "bowser_organ", "course_start", "piranha_music_box")


def _check_cancel(cancel_check: Callable[[], object]) -> None:
    if cancel_check():
        raise InterruptedError("CTL setup was cancelled")


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _file_sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _aggregate_hash(paths: list[pathlib.Path], base: pathlib.Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda p: p.relative_to(base).as_posix()):
        rel = path.relative_to(base).as_posix().encode("utf-8")
        digest.update(rel + b"\0" + bytes.fromhex(_file_sha256(path)) + b"\n")
    return digest.hexdigest()


def _load_module(name: str, path: pathlib.Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import source tool {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _read_bank(path: pathlib.Path, assembler, defines: set[str]):
    raw = path.read_text(encoding="utf-8")
    bank = assembler.orderedJsonDecoder.decode(assembler.strip_comments(raw))
    bank = assembler.apply_ifs(bank, defines)
    assembler.apply_version_diffs(bank, defines)
    assembler.normalize_sound_json(bank)
    return bank


def _sample_names(bank) -> set[str]:
    found: set[str] = set()

    def visit(value):
        if isinstance(value, dict):
            if isinstance(value.get("sample"), str):
                found.add(value["sample"])
            for key, item in value.items():
                if key != "sample":
                    visit(item)
        elif isinstance(value, list):
            for item in value:
                visit(item)

    visit(bank)
    return found


def _chunks(data: bytes):
    if len(data) < 12 or data[:4] != b"FORM" or data[8:12] not in (b"AIFF", b"AIFC"):
        raise ValueError("custom sample is not a valid AIFF/AIFC FORM")
    form_size = struct.unpack(">I", data[4:8])[0]
    if form_size + 8 > len(data) or form_size + 8 < 12:
        raise ValueError("truncated AIFF/AIFC FORM")
    pos = 12
    form_end = form_size + 8
    physical_end = len(data)
    parsed = []
    trailing_id3 = False
    while pos + 8 <= physical_end:
        kind = data[pos : pos + 4]
        size = struct.unpack(">I", data[pos + 4 : pos + 8])[0]
        start = pos + 8
        stop = start + size
        if stop > physical_end:
            raise ValueError(f"truncated {kind.decode('ascii', 'replace')} chunk")
        padded_stop = stop + (size & 1)
        # Some bundled AIFFs carry a trailing ID3 tag whose bytes are outside
        # (or overlap the last 8 bytes of) the FORM size. The encoder ignores
        # it; accept only a complete trailing ID3 chunk and leave it out.
        if kind == b"ID3 " and padded_stop == physical_end:
            pos = padded_stop
            trailing_id3 = True
            continue
        # The bundled encoder can leave FORM's declared size short by the
        # complete APPL/VADPCMCODES chunk. Walk the physical chunk sequence;
        # each individual chunk must still fit inside the file.
        parsed.append((kind, data[start:stop]))
        pos = padded_stop
    if pos != physical_end:
        raise ValueError("malformed AIFF/AIFC chunk alignment")
    yield from parsed


def _custom_aifc_bytes(source: pathlib.Path, writer_type) -> bytes:
    """Make a metadata-only AIFC wrapper; SSND contains zeroed encoded frames."""
    contents = source.read_bytes()
    comm = None
    codebook = None
    loop_seen = False
    for kind, payload in _chunks(contents):
        if kind == b"COMM":
            if comm is not None:
                raise ValueError(f"duplicate COMM in {source.name}")
            comm = payload
        elif kind == b"APPL" and payload[:4] == b"stoc" and len(payload) >= 5:
            pstring_len = payload[4]
            tag_start = 5
            tag_end = tag_start + pstring_len
            if tag_end > len(payload):
                raise ValueError(f"malformed APPL tag in {source.name}")
            tag = payload[tag_start:tag_end]
            body_start = (tag_end + 1) & ~1
            if tag == b"VADPCMCODES":
                if codebook is not None:
                    raise ValueError(f"duplicate VADPCMCODES in {source.name}")
                codebook = payload[body_start:]
            elif tag == b"VADPCMLOOPS":
                loop_seen = True
    if comm is None or len(comm) < 18:
        raise ValueError(f"missing/short COMM in {source.name}")
    if contents[8:12] != b"AIFF":
        raise ValueError(f"expected uncompressed AIFF source: {source.name}")
    if codebook is None:
        raise ValueError(f"missing VADPCMCODES in {source.name}")
    if loop_seen:
        raise ValueError(f"unexpected VADPCMLOOPS in custom source {source.name}")
    channels, frames, bits = struct.unpack(">hIh", comm[:8])
    if channels != 1 or bits != 16:
        raise ValueError(f"unsupported channel/bit format in {source.name}")
    # A VADPCM frame stores 16 samples in 9 bytes; the repository encoder pads
    # its final frame and aligns SSND data to 16 bits.
    encoded_len = (9 * ((frames + 15) // 16) + 1) & ~1
    if encoded_len <= 0:
        raise ValueError(f"empty custom source {source.name}")

    writer = writer_type(io.BytesIO())
    writer.add_section(
        b"COMM",
        comm[:18] + b"VAPC" + writer.pstring(b"VADPCM ~4-1"),
    )
    writer.add_section(b"INST", b"\0" * 20)
    writer.add_custom_section(b"VADPCMCODES", codebook)
    writer.add_section(b"SSND", b"\0" * (8 + encoded_len))
    writer.finish()
    return writer.out.getvalue()


def _run_disassembler(disassembler, rom: pathlib.Path, out_samples: pathlib.Path,
                      out_banks: pathlib.Path, cancel_check: Callable[[], object]) -> None:
    original_write_aiff = disassembler.write_aiff
    original_argv = sys.argv
    original_cwd = pathlib.Path.cwd()

    def write_metadata_only(entry, filename):
        _check_cancel(cancel_check)
        path = pathlib.Path(filename).with_suffix(".aifc")
        path.parent.mkdir(parents=True, exist_ok=True)
        clone = disassembler.AifcEntry(bytes(len(entry.data)), entry.book, entry.loop)
        clone.name = entry.name
        clone.tunings = list(entry.tunings)
        with path.open("wb") as stream:
            disassembler.write_aifc(clone, stream)

    try:
        disassembler.write_aiff = write_metadata_only
        sys.argv = [
            str(disassembler.__file__), str(rom), str(CTL_OFFSET), str(CTL_SIZE),
            str(TBL_OFFSET), str(TBL_SIZE), str(out_samples), str(out_banks),
        ]
        os.chdir(out_samples.parent)
        disassembler.main()
    finally:
        os.chdir(original_cwd)
        sys.argv = original_argv
        disassembler.write_aiff = original_write_aiff


def _write_custom_metadata(source: pathlib.Path, destination: pathlib.Path,
                           writer_type, cancel_check: Callable[[], object]) -> None:
    _check_cancel(cancel_check)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(_custom_aifc_bytes(source, writer_type))


def generate(root, rom, workspace, cancel_check=lambda: None) -> bytes:
    """Return the exact 193,920-byte US CTL using only Python's standard library.

    ``root`` contains ``codex/windows/ctl_tools`` and ``ctl_inputs``. All
    intermediates are created below the caller-provided ``workspace``.
    ``cancel_check`` may return true or raise ``InterruptedError`` to cancel.
    """
    root = pathlib.Path(root).resolve()
    rom = pathlib.Path(rom).resolve()
    workspace = pathlib.Path(workspace).resolve()
    _check_cancel(cancel_check)

    tools_dir = root / "codex/windows/ctl_tools"
    inputs_dir = root / "codex/windows/ctl_inputs"
    bank_inputs = inputs_dir / "sound_banks"
    custom_inputs = inputs_dir / "samples"
    disassembler_path = tools_dir / "disassemble_sound.py"
    assembler_path = tools_dir / "assemble_sound.py"
    bank_files = sorted(bank_inputs.glob("*.json"))
    custom_files = sorted(custom_inputs.glob("*") )
    custom_aiiffs = sorted(custom_inputs.rglob("*.aiff"))
    if not disassembler_path.is_file() or not assembler_path.is_file():
        raise FileNotFoundError("bundled CTL assembler/disassembler source is missing")
    if len(bank_files) != 45:
        raise ValueError(f"expected 45 sound-bank JSONs; found {len(bank_files)}")
    if not custom_aiiffs:
        raise ValueError("bundled custom sample inputs are missing")
    if not rom.is_file() or rom.stat().st_size != 8 * 1024 * 1024:
        raise ValueError("the selected ROM is not an 8 MiB image")
    rom_sha1 = hashlib.sha1()
    with rom.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            _check_cancel(cancel_check)
            rom_sha1.update(block)
    if rom_sha1.hexdigest() != ROM_SHA1:
        raise ValueError("the selected ROM does not match the verified US source image")

    workspace.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="ctl-", dir=workspace))
    sound = work / "sound"
    recovered_samples = work / "recovered/samples"
    recovered_banks = work / "recovered/banks"
    staged_samples = sound / "samples"
    staged_banks = sound / "sound_banks"
    recovered_samples.mkdir(parents=True)
    recovered_banks.mkdir(parents=True)
    staged_samples.mkdir(parents=True)
    staged_banks.mkdir(parents=True)

    old_dont_write = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        disassembler = _load_module("owned_ctl_disassembler", disassembler_path)
        assembler = _load_module("owned_ctl_assembler", assembler_path)
    finally:
        sys.dont_write_bytecode = old_dont_write

    _run_disassembler(disassembler, rom, recovered_samples, recovered_banks, cancel_check)
    recovered_bank_files = sorted(recovered_banks.glob("*.json"))
    if len(recovered_bank_files) != 38:
        raise RuntimeError(f"ROM recovery yielded {len(recovered_bank_files)} banks, expected 38")
    if sum(1 for _ in recovered_samples.rglob("*.aifc")) != 219:
        raise RuntimeError("ROM recovery did not yield the verified 219 source sample wrappers")

    for bank_file in bank_files:
        _check_cancel(cancel_check)
        shutil.copy2(bank_file, staged_banks / bank_file.name)

    defines = {"VERSION_US"}
    recovered_bank_data = {
        path.stem.upper(): _read_bank(path, assembler, defines)
        for path in recovered_bank_files
    }
    candidate_banks = {path.stem: _read_bank(path, assembler, defines) for path in bank_files}

    rom_samplebanks: dict[str, str] = {}
    original_sources: dict[str, dict[str, str]] = defaultdict(dict)
    custom_samplebanks: set[str] = set()
    extended_samples: set[str] = set()
    skipped: list[str] = []
    conflicts: list[str] = []
    for bank_stem, bank in candidate_banks.items():
        prefix = bank_stem[:2].upper()
        samplebank = bank.get("sample_bank")
        if prefix not in recovered_bank_data:
            if isinstance(samplebank, str) and "custom" in samplebank:
                custom_samplebanks.add(samplebank)
            elif samplebank == "extended":
                extended_samples.update(_sample_names(bank))
            else:
                skipped.append(bank_stem)
            continue
        recovered_name = recovered_bank_data[prefix]["sample_bank"]
        prior = rom_samplebanks.setdefault(samplebank, recovered_name)
        if prior != recovered_name:
            raise RuntimeError(f"inconsistent ROM mapping for sample bank {samplebank}")
        for sample in _sample_names(bank):
            index = sample[:2].upper()
            if len(index) != 2 or any(c not in "0123456789ABCDEF" for c in index):
                raise ValueError(f"sample lacks a two-digit ROM index: {bank_stem}:{sample}")
            previous = original_sources[samplebank].setdefault(index, sample)
            if previous != sample:
                conflicts.append(f"{samplebank}/{index}:{previous}|{sample}")
    if skipped or conflicts:
        raise RuntimeError(f"unsupported ROM sample mapping (skipped={skipped}, conflicts={conflicts})")

    copied_original = 0
    for samplebank, index_map in sorted(original_sources.items()):
        source_dir = recovered_samples / rom_samplebanks[samplebank]
        target_dir = staged_samples / samplebank
        target_dir.mkdir(parents=True, exist_ok=True)
        for index, sample_name in sorted(index_map.items()):
            _check_cancel(cancel_check)
            source = source_dir / f"{index}.aifc"
            if not source.is_file():
                raise FileNotFoundError(f"ROM source sample missing: {samplebank}/{index}")
            shutil.copyfile(source, target_dir / f"{sample_name}.aifc")
            copied_original += 1

    if not custom_samplebanks.issubset({p.name for p in custom_files if p.is_dir()}):
        missing = sorted(custom_samplebanks - {p.name for p in custom_files if p.is_dir()})
        raise FileNotFoundError(f"custom sample bank directories are missing: {missing}")
    custom_count = 0
    for samplebank in sorted(custom_samplebanks):
        source_dir = custom_inputs / samplebank
        target_dir = staged_samples / samplebank
        for source in sorted(source_dir.glob("*.aiff")):
            _check_cancel(cancel_check)
            _write_custom_metadata(source, target_dir / (source.stem + ".aifc"),
                                   disassembler.AifcWriter, cancel_check)
            custom_count += 1

    extended_dir = staged_samples / "extended"
    extended_dir.mkdir(parents=True, exist_ok=True)
    for source_bank in EXTENDED_SOURCE_ORDER:
        source_dir = staged_samples / source_bank
        if not source_dir.is_dir():
            continue
        for source in sorted(source_dir.glob("*.aifc")):
            _check_cancel(cancel_check)
            destination = extended_dir / source.name
            if not destination.exists():
                shutil.copyfile(source, destination)
    available_extended = {p.stem for p in extended_dir.glob("*.aifc")}
    missing_extended = sorted(extended_samples - available_extended)
    if missing_extended:
        raise FileNotFoundError(f"extended bank references lack metadata sources: {missing_extended[:10]}")

    original_parse_aifc = assembler.parse_aifc
    original_fail = assembler.fail
    def cancellable_parse_aifc(data, name, fname):
        _check_cancel(cancel_check)
        return original_parse_aifc(data, name, fname)
    assembler.parse_aifc = cancellable_parse_aifc
    def cancellable_fail(message):
        # The inherited parser catches Exception and reports a malformed file.
        # Preserve a pending user cancellation before it prints that diagnosis.
        _check_cancel(cancel_check)
        return original_fail(message)
    assembler.fail = cancellable_fail

    old_argv = sys.argv
    old_cwd = pathlib.Path.cwd()
    try:
        sys.argv = [
            str(assembler_path), "sound/samples", "sound/sound_banks",
            "sound/sound_data.ctl", "sound/sound_data.ctl-header",
            "sound/sound_data.tbl", "sound/sound_data.tbl-header",
            "--endian", "little", "--bitwidth", "64", "-D", "VERSION_US",
        ]
        os.chdir(work)
        assembler.main()
    except SystemExit as exc:
        # The repository assembler wraps per-file parse errors and exits after
        # printing them. Preserve cancellation semantics when a callback was
        # the original cause; otherwise surface a normal setup error.
        _check_cancel(cancel_check)
        raise RuntimeError(f"sound CTL assembler exited with status {exc.code}") from exc
    finally:
        os.chdir(old_cwd)
        sys.argv = old_argv
        assembler.parse_aifc = original_parse_aifc
        assembler.fail = original_fail

    compressed_ctl = work / "sound/sound_data_compressed.ctl"
    if not compressed_ctl.is_file():
        raise RuntimeError("assembler did not produce its compressed CTL output")
    raw_ctl = zlib.decompress(compressed_ctl.read_bytes())
    actual_sha = _sha256(raw_ctl)
    proof = {
        "operation": "stdlib-only metadata reconstruction of US sound_data.ctl",
        "matches_expected": len(raw_ctl) == EXPECTED_CTL_SIZE and actual_sha == EXPECTED_CTL_SHA256,
        "external_executables": 0,
        "ctl_length": len(raw_ctl),
        "ctl_sha256": actual_sha,
        "expected_ctl_length": EXPECTED_CTL_SIZE,
        "expected_ctl_sha256": EXPECTED_CTL_SHA256,
        "rom_sha1": rom_sha1.hexdigest(),
        "rom_ranges": {"ctl": [CTL_OFFSET, CTL_SIZE], "tbl": [TBL_OFFSET, TBL_SIZE]},
        "tool_source_sha256": {
            "assemble_sound.py": _file_sha256(assembler_path),
            "disassemble_sound.py": _file_sha256(disassembler_path),
        },
        "sound_bank_json_count": len(bank_files),
        "sound_bank_json_aggregate_sha256": _aggregate_hash(bank_files, bank_inputs),
        "custom_sample_count": custom_count,
        "custom_aiff_aggregate_sha256": _aggregate_hash(custom_aiiffs, custom_inputs),
        "recovered_rom_bank_count": len(recovered_bank_files),
        "recovered_rom_sample_count": 219,
        "original_referenced_sample_count": copied_original,
        "custom_sample_banks": sorted(custom_samplebanks),
        "original_sample_bank_mapping": dict(sorted(rom_samplebanks.items())),
        "extended_referenced_sample_count": len(extended_samples),
        "transient_workspace": str(work),
        "tbl_output_discarded": True,
        "asset_bytes_in_proof": False,
    }
    proof_path = workspace / "owned_ctl_proof.json"
    proof_path.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
    if not proof["matches_expected"]:
        raise RuntimeError(
            f"reconstructed CTL did not match the verified target: {len(raw_ctl)} bytes, {actual_sha}"
        )
    return raw_ctl
