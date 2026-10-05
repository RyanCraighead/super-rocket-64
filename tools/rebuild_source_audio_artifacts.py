"""Offline maintainer recipe for the source-derived audio setup inputs.

The Windows setup does not run GNU tools. This script rebuilds the bundled
16,832-byte sequence prefix and 123 VADPCM AIFCs from a source checkout, then
checks them against audio_inputs/encoding-manifest.json.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib


PREFIX_SHA256 = "fa027210a5d81a122968a0161005cd9ecd75b35183f08963a22366f11dfd23c8"
PROGRAM_SHA256 = "a57d7636a282ffc2e4adb578e350919a614ec8d7fb0621f5c0db91bfb7ef18e7"
TOAD_SOURCE_SHA256 = "86f141c7d4a3dd2bf32a85ec739d1200b764d799e114d20db38f3ba82e9fd1be"
TOAD_STAGED_SHA256 = "ab733963b4ced6ec1787f8c51c8118fc980f79558147cf765c57d57ab2cd8ffc"
ENCODING_MANIFEST_SHA256 = "c1846f1f4ca8b659ad051cb83cc07cc194a36b4d7b3735ae2b12a245df160489"
SOUND_BANKS_CANONICAL_SHA256 = "e9812cbcbab1e3bdca478f0cfe7fca0791be0d560fc023ea2c90b4424beb1aae"
PINNED_SOURCE_HASHES = {
    "sound/sequences/00_sound_player.s": "c51046ed58d5efbbefd82cd06089f3cce540a192a26ed1bb8457963376ad751d",
    "include/seq_macros.inc": "8525db7d7bbe6d04d267e68942387840f0f4e4b5ef09b9ffb595ff5987acd90a",
    "include/seq_luigi.inc": "5b7dbda59a1a5a7ee3e09ba9975900c12f89763c0a364862524e1ff9295670c1",
    "include/seq_wario.inc": "69f01b2056813f199befb85e6b46da94f024279c27bf94a94dac3e44bb715f56",
    "include/seq_toad.inc": TOAD_SOURCE_SHA256,
    "tools/assemble_sound.py": "30444ab4c0ab3a1e3fe6fbb9d3fd2882b2cbc940599c375bac98cf6527b59ae5",
    "sound/sequences.json": "6c4bd683011c2bc902ecc6be151dd6801f1a8dea7fbe8263c23f0635d84a41a6",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_hash(path: Path) -> str:
    return sha256(path.read_bytes())


def canonical_crlf(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n").replace(b"\n", b"\r\n")


def canonical_text_hash(data: bytes) -> str:
    return sha256(canonical_crlf(data))


def run(command: list[str], *, cwd: Path, stdout=None) -> subprocess.CompletedProcess:
    return subprocess.run(command, cwd=cwd, check=True, stdout=stdout,
                          stderr=subprocess.PIPE)


def staged_toad_include(source: bytes) -> bytes:
    source = canonical_crlf(source)
    if sha256(source) != TOAD_SOURCE_SHA256:
        raise ValueError("seq_toad.inc changed from the pinned source")
    anchor = b".layer_toad_D33:"
    old = b"layer_note1 39, 0xaa, 100"
    new = b"layer_note1 39, 0xaa, 127"
    if source.count(anchor) != 1 or source.count(old) != 1:
        raise ValueError("Could not uniquely locate the staged Toad velocity adjustment")
    start = source.index(anchor)
    end = source.find(b"layer_end", start)
    if end < 0 or old not in source[start:end]:
        raise ValueError("The velocity directive is no longer inside .layer_toad_D33")
    staged = source.replace(old, new, 1)
    if sha256(staged) != TOAD_STAGED_SHA256:
        raise ValueError("Staged Toad include hash differs from the audited patch")
    return staged


def make_prefix(root: Path, work: Path, manifest: dict, owned_audio: dict) -> bytes:
    for relative, expected_hash in PINNED_SOURCE_HASHES.items():
        path = root / relative
        if not path.is_file() or canonical_text_hash(path.read_bytes()) != expected_hash:
            raise ValueError("Pinned sequence source is missing or changed: " + relative)

    include_dir = work / "include"
    sequence_dir = work / "sound/sequences/us"
    bank_dir = work / "sound/sound_banks"
    sound_dir = work / "sound"
    include_dir.mkdir(parents=True)
    sequence_dir.mkdir(parents=True)
    bank_dir.mkdir(parents=True)
    sound_dir.mkdir(exist_ok=True)

    for name in ("seq_macros.inc", "seq_luigi.inc", "seq_wario.inc"):
        (include_dir / name).write_bytes(canonical_crlf((root / "include" / name).read_bytes()))
    (include_dir / "seq_toad.inc").write_bytes(staged_toad_include((root / "include/seq_toad.inc").read_bytes()))

    sequence_source = work / "sound/sequences/00_sound_player.s"
    sequence_source.parent.mkdir(parents=True, exist_ok=True)
    sequence_source.write_bytes(canonical_crlf((root / "sound/sequences/00_sound_player.s").read_bytes()))
    obj = work / "00_sound_player.o"
    program = work / "00_sound_player.m64"
    run(["as", "--defsym", "VERSION_US=1", "--defsym", "BITS_64=1",
         "-I", str(include_dir), "-o", str(obj), str(sequence_source)], cwd=work)
    run(["objcopy", "-j", ".rodata", str(obj), "-O", "binary", str(program)], cwd=work)
    program_bytes = program.read_bytes()
    if len(program_bytes) != 16_252 or sha256(program_bytes) != PROGRAM_SHA256:
        raise ValueError("Assembled 00 sound-player data differs from the audited source build")

    sequences_json = root / "sound/sequences.json"
    (sound_dir / "sequences.json").write_bytes(canonical_crlf(sequences_json.read_bytes()))
    bank_files = sorted((root / "sound/sound_banks").glob("*.json"))
    bank_hash = hashlib.sha256()
    for bank_file in bank_files:
        relative = bank_file.relative_to(root).as_posix()
        data = canonical_crlf(bank_file.read_bytes())
        bank_hash.update(relative.encode("utf-8") + b"\0" + bytes.fromhex(sha256(data)) + b"\n")
        (bank_dir / bank_file.name).write_bytes(data)
    if len(bank_files) != 45 or bank_hash.hexdigest() != SOUND_BANKS_CANONICAL_SHA256:
        raise ValueError("Pinned sound-bank JSON sources are missing or changed")
    (sequence_dir / "00_sound_player.m64").write_bytes(program_bytes)

    ranges = owned_audio["groups"]["music"]["ranges"]
    if len(ranges) != 34:
        raise ValueError("Expected 34 declared US sequence ROM spans")
    for span in ranges:
        symbol = span.get("symbol", "")
        if not symbol.startswith("SEQUENCE_us_") or not symbol.endswith("_m64"):
            raise ValueError("Unexpected US sequence symbol in owned-audio recipe")
        name = symbol[len("SEQUENCE_us_"):-len("_m64")]
        size = span.get("size")
        if type(size) is not int or size <= 0:
            raise ValueError("Invalid ROM sequence placeholder size")
        (sequence_dir / (name + ".m64")).write_bytes(bytes(size))

    input_files = [str(p) for p in sorted(sequence_dir.glob("*.m64"))]
    expected_inputs = 35
    if len(input_files) != expected_inputs:
        raise ValueError(f"Expected {expected_inputs} sequence input files, found {len(input_files)}")
    assembler = root / "tools/assemble_sound.py"
    command = [sys.executable, str(assembler), "--endian", "little", "--bitwidth", "64",
               "--sequences", "sound/sequences.bin", "sound/sequences-header.bin",
               "sound/bank_sets", "sound/sound_banks", "sound/sequences.json",
               "-D", "VERSION_US", *input_files]
    run(command, cwd=work)
    compressed = sound_dir / "sequences_compressed.bin"
    prefix = zlib.decompress(compressed.read_bytes())
    if len(prefix) != 16_832 or sha256(prefix) != PREFIX_SHA256:
        raise ValueError("Generated music prefix differs from its audited target")
    return prefix


def rebuild_samples(root: Path, work: Path, output: Path, manifest: dict) -> list[dict]:
    gcc = shutil.which("gcc")
    if not gcc:
        raise FileNotFoundError("gcc is required for the offline source-artifact rebuild")
    tools_dir = work / "tools"
    tools_dir.mkdir()
    extractor = tools_dir / "aiff_extract_codebook"
    encoder = tools_dir / "vadpcm_enc"
    source_dir = work / "encoder-source"
    source_dir.mkdir()
    adpcm = source_dir / "tools/sdk-tools/adpcm"
    adpcm.mkdir(parents=True)
    source = source_dir / "tools/aiff_extract_codebook.c"
    source.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / "tools/aiff_extract_codebook.c", source)
    for name in ("vadpcm_enc.c", "vpredictor.c", "quant.c", "util.c", "vencode.c", "vadpcm.h"):
        source_text = root / "tools/sdk-tools/adpcm" / name
        (adpcm / name).write_bytes(canonical_crlf(source_text.read_bytes()))
    source.write_bytes(canonical_crlf(source.read_bytes()))
    run([gcc, "-O2", "-o", str(extractor), str(source), "-lm"], cwd=work)
    run([gcc, "-O2", "-I", str(adpcm), "-o", str(encoder),
         str(adpcm / "vadpcm_enc.c"), str(adpcm / "vpredictor.c"),
         str(adpcm / "quant.c"), str(adpcm / "util.c"),
         str(adpcm / "vencode.c"), "-lm"], cwd=work)

    rows = manifest["samples"]
    if len(rows) != 123:
        raise ValueError("Expected exactly 123 pinned source AIFFs")
    for relative, record in manifest.get("tool_source_sha256", {}).items():
        path = root / relative
        if not path.is_file() or canonical_text_hash(path.read_bytes()) != record.get("sha256"):
            raise ValueError("VADPCM tool source differs from its pinned hash: " + relative)
    sample_root = root / "codex/windows/ctl_inputs/samples"
    results = []
    for row in rows:
        source_rel = PurePosixPath(row["source"])
        encoded_rel = PurePosixPath(row["encoded"])
        if source_rel.parts[:2] != ("sound", "samples") or encoded_rel.parts[:1] != ("encoded",):
            raise ValueError("Manifest has an unexpected source or encoded path")
        rel = PurePosixPath(*source_rel.parts[2:])
        source_path = sample_root.joinpath(*rel.parts)
        if not source_path.is_file() or source_path.stat().st_size != row["source_size"] or file_hash(source_path) != row["source_sha256"]:
            raise ValueError("Source AIFF differs from its pinned input hash: " + rel.as_posix())
        dest_rel = PurePosixPath(*encoded_rel.parts[1:])
        dest_path = output / "encoded" / Path(*dest_rel.parts)
        dest_path.parent.mkdir(parents=True, exist_ok=True)
        table_path = work / "codebook.table"
        with table_path.open("wb") as table:
            run([str(extractor), str(source_path)], cwd=work, stdout=table)
        if table_path.stat().st_size != 200:
            raise ValueError("Source AIFF lacks the expected VADPCM codebook: " + rel.as_posix())
        run([str(encoder), "-c", str(table_path), str(source_path), str(dest_path)], cwd=work)
        if dest_path.stat().st_size != row["encoded_size"] or file_hash(dest_path) != row["encoded_sha256"]:
            raise ValueError("Encoded AIFC differs from its pinned output hash: " + rel.as_posix())
        results.append({"source": rel.as_posix(), "encoded": dest_rel.as_posix(),
                         "source_sha256": row["source_sha256"],
                         "encoded_sha256": row["encoded_sha256"]})
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path,
                        help="Pinned audio_inputs/encoding-manifest.json")
    parser.add_argument("--owned-audio-recipe", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path,
                        help="A new output directory for regenerated audio inputs")
    args = parser.parse_args()
    root = args.source_root.resolve()
    out = args.output.absolute()
    if out.exists():
        raise FileExistsError("Output directory already exists; choose a new path")
    if not shutil.which("as") or not shutil.which("objcopy"):
        raise FileNotFoundError("GNU as and objcopy are required for the offline sequence build")
    manifest_raw = args.manifest.read_bytes()
    if sha256(manifest_raw) != ENCODING_MANIFEST_SHA256:
        raise ValueError("Pinned audio encoding manifest changed")
    manifest = json.loads(manifest_raw)
    if manifest.get("format") != "sr64-audio-encoding@1":
        raise ValueError("Unsupported audio encoding manifest")
    if manifest.get("sample_count") != 123 or manifest.get("private_engine_or_build_outputs_read") is not False:
        raise ValueError("Audio encoding manifest does not describe the audited source-only build")
    owned = json.loads(args.owned_audio_recipe.read_text(encoding="utf-8"))
    if canonical_text_hash(args.owned_audio_recipe.read_bytes()) != "550f84442825ed1704ccd4c6ae5dd4877d4bb5c586872fcd4dc58d93ad5739cd":
        raise ValueError("Owned-audio recipe changed from the audited US span set")
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=out.name + ".stage-", dir=out.parent) as temp:
        temp_root = Path(temp)
        artifact_root = temp_root / "artifacts"
        artifact_root.mkdir()
        work = temp_root / "work"
        work.mkdir()
        prefix = make_prefix(root, work, manifest, owned)
        (artifact_root / "sound-player-prefix.bin").write_bytes(prefix)
        encoded_results = rebuild_samples(root, work, artifact_root, manifest)
        shutil.copyfile(args.manifest, artifact_root / "encoding-manifest.json")
        record = {
            "format": "sr64-audio-source-build@1",
            "source_root": "read-only",
            "private_full_buffer_seeds_used": False,
            "toad_staging_patch": {
                "source": "include/seq_toad.inc",
                "anchor": ".layer_toad_D33:",
                "replace": "layer_note1 39, 0xaa, 100",
                "with": "layer_note1 39, 0xaa, 127",
                "staged_only": True,
            },
            "prefix": {"size": len(prefix), "sha256": sha256(prefix)},
            "encoded_sample_count": len(encoded_results),
            "encoded_files": encoded_results,
            "encoding_manifest_sha256": sha256(manifest_raw),
        }
        (artifact_root / "build-record.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        artifact_root.replace(out)
    print(json.dumps({"status": "built", "output": str(out),
                      "prefix_sha256": PREFIX_SHA256, "encoded_samples": 123}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
