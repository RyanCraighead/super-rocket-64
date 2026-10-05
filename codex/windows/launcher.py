#!/usr/bin/env python3
"""Shared local validators and character extraction helpers.

The public edition's seven_launcher entry point owns setup, verified tool
provisioning and explicit play. These helpers never modify original inputs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "codex" / "oot" / "assets"))
sys.path.insert(0, str(ROOT / "codex" / "oot" / "mesh"))
sys.path.insert(0, str(ROOT / "codex"))
from extract_oot_assets import AssetError, extract, identify_rom, normalize_rom
from decode_link_mesh import convert
# Package-qualified imports keep each game's pose/animation helpers separate.
from bm64.assets.prepare_bm64 import read_rom as read_bm64_rom, validate as identify_bm64
from bm64.assets.prepare_bm64 import EXPECTED_SHA1 as BM64_SHA1, EXPECTED_MD5 as BM64_MD5
from bm64.mesh.decode_bm64_mesh import convert as convert_bm64
from bk.assets.prepare_bk import read_rom as read_bk_rom, validate as identify_bk
from bk.mesh.decode_bk_mesh import convert as convert_bk
from windows.prepare_spiderman import convert as convert_spiderman, validate as validate_spiderman
from windows.prepare_spiderman import check_prerequisite as check_spiderman_prerequisite
from windows.prepare_spiderman import REQUIRED as SPIDERMAN_REQUIRED, ROM_SHA256 as SPIDERMAN_SHA256

SM64_SIZE = 8 * 1024 * 1024
SM64_MD5 = "20b854b239203baf6c961b850a4a51a2"
SM64_SHA1 = "9bef1128717f958171a4afac3ed78ee2bb4e86ce"
OOT_MD5 = "57a9719ad547c516342e1a15d5c28c3d"
OOT_SHA1 = "41b3bdc48d98c48529219919015a1af22f5057c2"
OOT_SHA256 = "49acd3885f13b0730119b78fb970911cc8aba614fe383368015c21565983368d"
BK_SIZE = 16 * 1024 * 1024
BK_SHA1 = "ded6ee166e740ad1bc810fd678a84b48e245ab80"
BK_SHA256 = "f9ad43f64c3a38b0ca4067e24f570dcb8f3f8fec077c5c342484bcbec7ff6d22"
MAX_ROM_BYTES = 64 * 1024 * 1024
CHARACTERS = ("link", "mario", "bomberman", "banjo", "spiderman")
PROFILE_PATHS = {"link": ".runtime/windows", "mario": ".runtime/windows-mario",
                 "bomberman": ".runtime/windows-bomberman", "banjo": ".runtime/windows-banjo",
                 "spiderman": ".runtime/windows-spiderman"}
BM64_MODELS = ("player", "bomb_00", "bomb_01", "bomb_02", "bomb_03", "bomb_04", "bomb_05",
               "effect_06", "effect_16", "effect_17")


class SetupError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise SetupError(message)


def is_redirect(path):
    """Reject symlinks and Windows reparse points, including junctions (3.10+)."""
    info = path.lstat()
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & 0x400)


def private_path(root, relative):
    """Resolve a package-owned path without following an existing redirection."""
    root = Path(root).resolve()
    rel = Path(relative)
    require(not rel.is_absolute() and all(p not in (".", "..") for p in rel.parts),
            "Private path must stay beneath the package")
    current = root
    for component in rel.parts:
        current = current / component
        if current.exists() or current.is_symlink():
            require(not is_redirect(current), "Private runtime cannot contain symlinks or junctions")
    require(current.resolve().is_relative_to(root), "Private runtime escaped the package")
    return current


def check_location(root=ROOT):
    # The upstream game still has narrow/legacy MAX_PATH file I/O, even though
    # the new child-process transport supports UTF-16 Windows paths.
    root = Path(root).resolve()
    require(str(root).isascii() and len(str(root)) <= 100,
            r"Use a short ASCII-only package folder, at most 100 characters, such as C:\Games\SuperRocket64 (spaces are supported)")


def read_bounded(path, maximum):
    path = Path(path)
    require(path.is_file(), "Select an existing local ROM file")
    require(path.stat().st_size <= maximum, "Input file exceeds its size limit")
    with path.open("rb") as handle:
        value = handle.read(maximum + 1)
    require(len(value) <= maximum, "Input file changed or exceeds its size limit")
    return value


def validate_sm64(path):
    raw = read_bounded(path, SM64_SIZE)
    require(len(raw) == SM64_SIZE, "SM64 must be the original 8 MiB US ROM")
    rom, _order = normalize_rom(raw)
    require(hashlib.md5(rom).hexdigest() == SM64_MD5 and hashlib.sha1(rom).hexdigest() == SM64_SHA1,
            "Unsupported SM64 ROM: exact original US release required; no files changed")
    return rom


def validate_oot_identity(identity):
    require(identity.get("profile") == "ntsc-1.2-US" and
            identity.get("normalized_md5") == OOT_MD5 and
            identity.get("normalized_sha1") == OOT_SHA1 and
            identity.get("normalized_sha256") == OOT_SHA256,
            "This package requires the original US OoT 1.2 compressed retail ROM")


def validate_oot(path):
    _rom, identity = identify_rom(read_bounded(path, MAX_ROM_BYTES))
    validate_oot_identity(identity)
    return identity


def validate_bm64_identity(identity):
    require(isinstance(identity, dict) and
            identity.get("profile") == "bm64-us-1.0" and identity.get("size") == 8 * 1024 * 1024 and
            identity.get("normalized_sha1") == BM64_SHA1 and identity.get("normalized_md5") == BM64_MD5,
            "This package requires original Bomberman 64 (1997), USA 1.0")


def validate_bm64(path):
    _rom, identity = identify_bm64(read_bm64_rom(Path(path)))
    validate_bm64_identity(identity)
    return identity


def validate_character_inputs(character, oot_path=None, bm64_path=None, bk_path=None, spiderman_path=None):
    require((oot_path is None or character == "link") and
            (bm64_path is None or character == "bomberman") and
            (bk_path is None or character == "banjo") and
            (spiderman_path is None or character == "spiderman"),
            "--oot is only for Link; --bm64 is only for Bomberman; --bk is only for Banjo; --spiderman is only for Spider-Man")


def validate_bk_identity(identity):
    require(isinstance(identity, dict) and
            identity.get("profile") == "bk-us-rev1" and type(identity.get("size")) is int and
            identity["size"] == BK_SIZE and
            identity.get("normalized_sha1") == BK_SHA1 and identity.get("normalized_sha256") == BK_SHA256,
            "This package requires original Banjo-Kazooie USA Rev 1, exactly 16 MiB")


def validate_bk(path):
    _rom, identity = identify_bk(read_bk_rom(Path(path)))
    validate_bk_identity(identity)
    return identity


def profile_path(root, character):
    require(character in CHARACTERS, "Choose link, mario, bomberman, banjo, or spiderman")
    return private_path(root, PROFILE_PATHS[character])


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_setup_inventory(stage, character="link"):
    files = {}
    for item in sorted(stage.rglob("*")):
        if item.is_file():
            files[item.relative_to(stage).as_posix()] = {"size": item.stat().st_size, "sha256": sha256(item)}
    inventory = {"schema_version": 1, "files": files}
    if character != "link":
        inventory.update(schema_version=2, character=character)
    (stage / "setup.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")


def check_assets(root=ROOT, character="link"):
    return check_asset_directory(profile_path(root, character), character)


def check_asset_directory(target, character):
    inventory_path = private_path(target, "setup.json")
    require(inventory_path.is_file(), f"Local {character} assets are not set up. Run setup --character {character} first")
    inventory = json.loads(read_bounded(inventory_path, 1024 * 1024).decode("utf-8"))
    require(isinstance(inventory, dict), "Unsupported setup inventory")
    valid_profile = (inventory.get("schema_version") == 1 and character == "link") or (
        inventory.get("schema_version") == 2 and inventory.get("character") == character)
    require(valid_profile and isinstance(inventory.get("files"), dict),
            "Unsupported setup inventory; keep this package and its private data together")
    files = inventory["files"]
    require(1 <= len(files) <= 2000, "Invalid setup inventory size")
    required = ["save/baserom.us.z64"]
    allowed = ("save/",)
    if character == "link":
        required += ["oot-link/manifest.json", "oot-link/mesh-adult/mesh.json"]
        allowed += ("oot-link/",)
    elif character == "spiderman":
        required += ["spiderman-original/" + name for name in SPIDERMAN_REQUIRED]
        allowed += ("spiderman-original/",)
    elif character == "banjo":
        required += ["bk-duo/manifest.json", "bk-duo/player/mesh.json", "bk-duo/player/animations.json"]
        allowed += ("bk-duo/",)
    elif character == "bomberman":
        required += ["bm64-bomberman/manifest.json"]
        required += [f"bm64-bomberman/{name}/{leaf}" for name in BM64_MODELS
                     for leaf in ("mesh.json", "animations.json")]
        allowed += ("bm64-bomberman/",)
    for key in required:
        require(key in files, "Incomplete local asset setup")
    for relative, entry in files.items():
        # Use POSIX names in the portable inventory; forbid backslash/drive tricks.
        require(isinstance(relative, str) and not any(x in relative for x in ("\\", ":")) and
                relative.startswith(allowed), "Invalid setup inventory path")
        item = private_path(target, relative)
        require(item.is_file() and isinstance(entry, dict) and item.stat().st_size == entry.get("size") and
                sha256(item) == entry.get("sha256"),
                "A local asset is missing or changed. Keep saves, and restore a fresh package/setup")
    validate_sm64(target / "save" / "baserom.us.z64")
    if character == "link":
        manifest = json.loads(read_bounded(target / "oot-link" / "manifest.json", 1024 * 1024).decode("utf-8"))
        validate_oot_identity(manifest.get("rom", {}))
    elif character == "spiderman":
        model = json.loads(read_bounded(target / "spiderman-original/spiderman_model.json", 2 * 1024 * 1024).decode("utf-8"))
        require(isinstance(model, dict) and model.get("schema") == "n64codexlab.spiderman.original.v1"
                and isinstance(model.get("source"), dict) and model["source"].get("rom_sha256") == SPIDERMAN_SHA256
                and isinstance(model.get("original_pose_execution"), dict), "Unsupported original Spider-Man model or poses")
        require(files.get("spiderman-original/.gitignore") == {"size": 2, "sha256": hashlib.sha256(b"*\n").hexdigest()},
                "Missing Spider-Man private-output guard")
        fingerprints = json.loads((Path(__file__).parent / "spiderman-runtime-fingerprints.json").read_text(encoding="utf-8"))
        require(fingerprints.get("schema_version") == 1 and fingerprints.get("rom_sha256") == SPIDERMAN_SHA256
                and isinstance(fingerprints.get("files"), dict), "Invalid packaged Spider-Man fingerprints")
        expected = {"spiderman-original/" + name: item for name, item in fingerprints["files"].items()}
        require({name: item for name, item in files.items() if name.startswith("spiderman-original/")} == expected,
                "Spider-Man extracted files differ from the verified original runtime fingerprints")
        require(set(files) == set(expected) | {"save/baserom.us.z64"}, "Unexpected Spider-Man setup inventory")
    elif character == "banjo":
        check_bk_manifest(target, files)
    elif character == "bomberman":
        manifest = json.loads(read_bounded(target / "bm64-bomberman" / "manifest.json", 1024 * 1024).decode("utf-8"))
        require(isinstance(manifest, dict) and manifest.get("schema_version") == "bm64-original-assets-v1",
                "Unsupported BM64 asset manifest")
        validate_bm64_identity(manifest.get("identity", {}))
        models = manifest.get("models", [])
        require(isinstance(models, list) and len(models) == len(BM64_MODELS) and
                {entry.get("name") for entry in models if isinstance(entry, dict)} == set(BM64_MODELS),
                "Incomplete original BM64 model set")
        for entry in models:
            require(entry.get("mesh") == entry["name"] + "/mesh.json", "Unexpected BM64 mesh path")
            require(entry.get("sha256") == files["bm64-bomberman/" + entry["mesh"]]["sha256"],
                    "BM64 mesh manifest does not match the local inventory")
    return target


def check_bk_manifest(target, inventory_files):
    manifest = json.loads(read_bounded(target / "bk-duo" / "manifest.json", 1024 * 1024).decode("utf-8"))
    require(isinstance(manifest, dict) and manifest.get("schema_version") == "bk-original-assets-v1",
            "Unsupported Banjo-Kazooie asset manifest")
    validate_bk_identity(manifest.get("identity", {}))
    models = manifest.get("models")
    require(isinstance(models, list) and len(models) == 1 and isinstance(models[0], dict) and
            models[0].get("name") == "player" and models[0].get("mesh") == "player/mesh.json" and
            models[0].get("animations") == "player/animations.json",
            "Incomplete original Banjo-Kazooie player model")
    entries = manifest.get("files")
    require(isinstance(entries, list) and 3 <= len(entries) <= 1998, "Invalid Banjo-Kazooie file manifest")
    expected = {"save/baserom.us.z64", "bk-duo/manifest.json", "bk-duo/.gitignore"}
    require(inventory_files.get("bk-duo/.gitignore") == {"size": 2, "sha256": hashlib.sha256(b"*\n").hexdigest()},
            "Missing Banjo-Kazooie private-output guard")
    for entry in entries:
        require(isinstance(entry, dict), "Invalid Banjo-Kazooie manifest entry")
        relative = entry.get("path")
        require(isinstance(relative, str) and (
            relative in ("player/mesh.json", "player/animations.json") or
            re.fullmatch(r"player/textures/tex_[0-9a-f]{24}\.rgba", relative)),
            "Unexpected Banjo-Kazooie asset path")
        name = "bk-duo/" + relative
        require(name not in expected, "Duplicate Banjo-Kazooie manifest path")
        expected.add(name)
        require(type(entry.get("size")) is int and entry["size"] > 0 and
                isinstance(entry.get("sha256"), str) and re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]),
                "Invalid Banjo-Kazooie manifest size or digest")
        require(inventory_files.get(name) == {"size": entry["size"], "sha256": entry["sha256"]},
                "Banjo-Kazooie file manifest does not match the local inventory")
    require(set(inventory_files) == expected, "Unexpected or incomplete Banjo-Kazooie asset inventory")








def clean_environment(environment):
    allowed = {"PATH", "SYSTEMROOT", "WINDIR", "PATHEXT", "LANG", "LC_ALL", "TEMP", "TMP",
               "USERPROFILE", "HOMEDRIVE", "HOMEPATH", "APPDATA", "LOCALAPPDATA"}
    return {key: value for key, value in environment.items() if key.upper() in allowed}






def check_package(root=ROOT):
    root = Path(root).resolve()
    for relative in ("sm64coopdx.exe", "lang/English.ini"):
        require((root / relative).is_file(), f"Package file missing: {relative}. Extract the complete ZIP first")
    # Prove the expected binary architecture without executing it.
    with (root / "sm64coopdx.exe").open("rb") as handle:
        dos = handle.read(64)
        require(len(dos) == 64 and dos[:2] == b"MZ", "The game is not a Windows executable")
        offset = struct.unpack_from("<I", dos, 60)[0]
        require(offset <= 1024 * 1024, "Invalid Windows executable header")
        handle.seek(offset)
        signature = handle.read(6)
        require(signature == b"PE\0\0\x64\x86", "This package requires the Windows x86-64 game binary")










