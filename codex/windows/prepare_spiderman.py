"""Prepare the exact Spider-Man USA 1.0 runtime from the user's local cartridge.

All derived game data remains in the caller's private staging directory. No
network, subprocess, download, installer, login or game launch is performed.
Original cached-frame poses are computed by bounded Unicorn MIPS execution of
code read only from the verified user cartridge; no original code is bundled.
"""
from contextlib import contextmanager
import importlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from codex.spiderman.assets.archive import load_rom, ROM_SHA256

REQUIRED = (
    "spiderman_model.json", "original-markers.json", "combat.bin", "boot.bin",
    "web-textures.json", "web_texture_46_mip_0.rgba", "web_texture_47_mip_0.rgba",
    "trail-texture.json", "trail_texture_41_mip_0.rgba", "web-attack-textures.json",
    "web_attack_texture_38_mip_0.rgba", "web_attack_texture_108_mip_0.rgba",
    "web_attack_texture_111_mip_0.rgba", "dome/dome_assets.json",
    "dome/texture_403_0.rgba", "dome/texture_404_0.rgba",
) + tuple(f"animations/anim_{i:03}.json" for i in range(300)) + tuple(
    f"dome/{stem}.{suffix}" for stem in ("webdome2_0",) + tuple(f"webdome3_{i}" for i in range(5))
    for suffix in ("vtxbe", "cornersbe"))


def validate(path):
    load_rom(path)  # Exact 32 MiB big-endian USA 1.0 SHA-256; bounded single-ROM ZIP accepted.
    return {"rom_sha256": ROM_SHA256}


def check_prerequisite():
    try:
        import unicorn
        if unicorn.__version__ != "2.1.4":
            raise ImportError("Unverified Unicorn version")
        from unicorn import UC_ARCH_MIPS
        if not unicorn.uc_arch_supported(UC_ARCH_MIPS):
            raise ImportError("This Unicorn build has no MIPS backend")
    except (ImportError, OSError) as error:
        raise ValueError("The bundled character runtime is missing or invalid (Unicorn 2.1.4 with MIPS support). "
                         "Restore the verified launcher program files and retry setup. Keep your original ROMs and saved data.") from error


@contextmanager
def arguments(values):
    previous = sys.argv
    sys.argv = values
    try:
        yield
    finally:
        sys.argv = previous


def convert(rom_path, output):
    check_prerequisite()
    validate(rom_path)
    output = Path(output)
    if output.exists():
        raise ValueError("Spider-Man extraction output must be a new staging directory")
    if ".runtime" not in output.resolve().parts:
        raise ValueError("Spider-Man data must stay in private .runtime storage")
    from codex.spiderman.mesh.decode_spiderman import export
    from codex.spiderman.mesh.export_original_poses import run as export_poses
    from codex.spiderman.markers.decode_markers import export as export_markers
    from codex.spiderman.combat.extract_combat import decode, encode
    from codex.spiderman.dome_render.export_assets import export as export_dome
    export(rom_path, output)
    print("Computing all 4,196 original cached-frame poses; this can take several minutes...", flush=True)
    evidence = export_poses(rom_path, output)
    if evidence["frame_count"] != 4196 or evidence["bone_pose_count"] != 75528:
        raise ValueError("Incomplete original pose set")
    export_markers(rom_path, output / "original-markers.json")
    (output / "combat.bin").write_bytes(encode(decode((output / "boot.bin").read_bytes())))
    for module in ("web.export_web_textures", "trail_render.export_trail_texture", "web_attack_render.export_textures"):
        with arguments([module, "--rom", str(rom_path), "--out", str(output)]):
            importlib.import_module("codex.spiderman." + module).main()
    export_dome(rom_path, output / "dome")
    # Discard only our own intermediate cartridge/archive/code copies. Runtime
    # needs its verified boot sidecar for recovered table lookup, as well as models,
    # poses, textures, markers and combat data, but not the other archive caches.
    for name in ("archive.json", "bounds.bin", "objects.bin", "renderbank.bin",
                 "spidey.psx.n64", "pose_samples.json", "original-pose-evidence.json"):
        (output / name).unlink(missing_ok=True)
    for path in output.glob("group*.bin"):
        path.unlink()
    # Make Python text output deterministic across Windows and Linux, including
    # the preexisting exporters which default to the host newline convention.
    for path in output.rglob("*.json"):
        value = path.read_text(encoding="utf-8")
        path.write_bytes(value.encode("utf-8"))
    for relative in REQUIRED:
        if not (output / relative).is_file():
            raise ValueError("Missing original runtime asset: " + relative)
    (output / ".gitignore").write_bytes(b"*\n")
    return {"rom_sha256": ROM_SHA256}
