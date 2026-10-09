# Installed car appearance migration

Verified launcher startup, including the restart after an update, automatically
checks the installed Octane cache and adds the improved body materials when the
supported local source packages are available. Normal setup completion uses the
same migration. Users do not need to repeat setup, import a model, or opt in on a
separate page. Existing valid materials are reused without conversion.

Local Epic/Steam metadata supplies candidates; both the body and Startup package
hashes must match. The wheel package is not required when the installed geometry
is verified. Missing sources, unsupported versions, insufficient storage or a
failed conversion leave the known working geometry untouched and keep Play
available. Only then does Play show **Finish car appearance**, which opens the
source/retry page. **Characters > Car appearance** remains a compact route to
retry or revert. No source location is written to new preferences.

`appearance-migrate` is distinct from read-only `wizard-status` and
`appearance-status`. Startup defers migration if a game is active. The helper
also checks game/setup locks, acquires the existing setup lock and checks again
before activation. Cancellation happens before the atomic material rename;
it does not terminate a game or replace a partially generated material folder.
Conversion uses the existing pinned tool, downloading it only if needed, and
bounded temporary compatibility copies of the owned packages.

Only these files are activated under
`data/.runtime/windows-octane/octane-model/materials`:

- `body.uv`
- `body.png`
- `chassis.png`
- `manifest.json`

**Revert appearance** atomically renames that validated directory to
`materials.disabled`. This archive is also the explicit previous-appearance
choice: later startup and setup preserve it. **Use improved appearance** moves
the verified archive back without a source or download. The operation rejects
extra files, redirected paths, modified materials and conflicting archives,
rather than overwrite possible customization. It never rewrites setup inventory,
mesh geometry, audio, ROMs, saves, controls or launcher preferences.

The release payload manifest has a `car_materials` capability containing the
material schema, canonical profile SHA-256 and the bundled engine SHA-256.
`tools/build_release_payload.py` emits it. The helper verifies that pairing and
the engine bytes before conversion and immediately before activation. Release
integration must keep this field, the helper, its CLI routing and the UI together,
and build/test the engine with the corresponding material renderer. A UI-only
rebuild around an older unchanged payload is insufficient.

Regression commands:

- `python -B -m unittest discover -s codex/windows/tests -p "test_car_*.py"`
- `powershell -NoProfile -File launcher/test-headless.ps1`
- `powershell -NoProfile -File launcher/test-desktop.ps1`
- `powershell -NoProfile -File launcher/test-updates.ps1`
- `powershell -NoProfile -File launcher/test-persistence.ps1`

Desktop tests run on a separate desktop that is never activated. Synthetic
tests cover normal automatic startup, missing prerequisites, deferred migration,
conversion failure, incompatible/changed engines, cancellation, explicit revert,
source-free reapply, and exact unrelated-file preservation. Rendering remains
the v0.2.21 host approximation described in `codex/rocketleague/MATERIALS.md`.
No proprietary textures belong in source control or the public payload.
