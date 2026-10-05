# Build Super Rocket 64

Players only need the release EXE. The launcher includes Python and provisions
its pinned extraction tool; these instructions are for maintainers.

The supported maintainer build uses Linux/WSL, GNU make, GCC/G++, CMake 3.20+,
Python 3, MinGW-w64 x86-64, SDL2's MinGW development package, GLEW's Windows
development headers/library and MinGW zlib. The verified toolchain is Ubuntu
24.04 with MinGW GCC 13, SDL2 2.32.10 and GLEW 2.2.0. Dependencies are available
from their official projects or the distribution package manager.

Set `SDL_ROOT` to SDL2's `x86_64-w64-mingw32` directory and `GLEW_ROOT` to the
directory containing `include/GL` and `lib`, then run:

```sh
bash tools/build_windows.sh
```

The script validates the included RocketSim source snapshot, obtains and
SHA-256 checks the pinned public upstream Lua library, then builds with two
workers. It does not need a ROM, private repository, old object files or game
assets. Output: `.build/windows/us_pc/sm64coopdx.exe`. Discord SDK, CoopNet and
the updater are disabled; direct-IP networking remains available.

To reproduce the source-derived audio inputs, use GNU binutils and the host
compiler with the checked-in maintainer script:

```sh
python3 tools/rebuild_source_audio_artifacts.py --source-root . \
  --manifest codex/windows/audio_inputs/encoding-manifest.json \
  --owned-audio-recipe codex/windows/owned-audio-recipe.json \
  --output .build/regenerated-audio
```

This generates the original custom recordings' encodings and sound-player
prefix. It verifies their registered hashes. User setup supplies the separate
SM64 music/sample spans from the owned ROM; it does not invoke a compiler.

For a Windows release, put the three official, hash-pinned archives identified
in `codex/windows/single_exe/EMBEDDED-RUNTIME.txt` in a dependency-cache folder:

```sh
python3 tools/build_release_payload.py --engine .build/windows/us_pc/sm64coopdx.exe \
  --dependencies /path/to/dependency-cache --output /path/to/new-release
```

Or build the payload, compile the complete launcher and generate the update
manifest in one Windows PowerShell command:

```powershell
.\codex\windows\single_exe\build-release.ps1 -Engine .\.build\windows\us_pc\sm64coopdx.exe -Dependencies C:\build-cache -Output C:\release-output -Python python
```

The output contains the standalone Windows EXE and `Super-Rocket-64-update.json`.
Upload both to the matching numeric version tag. GitHub's release asset digests
must be present; the updater checks both digests and the manifest. Do not replace
the EXE or manifest of an existing release after publication.

Run the resulting EXE with `--verify-only --install-dir <new-empty-folder>` for
windowless package validation. The engine's `--verify-local-engine-data` mode
validates an existing local setup and exits before opening a window; set
`SUPER_ROCKET64_ENGINE_ASSETS` to that setup's shared-data directory.

Release checks must verify the package manifest, absence of local inputs and
private paths, notices/source obligations, and setup reuse/save preservation.
Keep two-PC and physical-controller coverage separate from automated checks.
