# Approved launcher presentation

This branch changes the production WinForms launcher under `codex/windows/single_exe`, based on public release v0.2.15 (`baf38b785e7bc21d098168f740c56af75820f972`). It does not change the in-game settings menu, engine, physics, networking, extraction rules, updater protocol, or save layout.

`Presentation.cs` and `PresentationPages.cs` arrange and paint the existing controls. Their original click handlers remain authoritative. The approved PNGs remain unmodified embedded resources; `Art/provenance.json` records their identities and hashes. Home uses the approved raster heading, artwork, button artwork, and canonical 1536×1024 geometry. The top-right readiness pill is omitted. The review annotation in the footer is replaced with the actual launcher version.

The shell also covers installation location, game sources, optional characters, progress, completion, online choice/host/join, settings, updates, and recovery. Optional-character revisions and source formats remain available in expanded rows. Readiness/repair badges follow the actual report. Update consent is still explicit. Operation cancellation is outside the disabled page container. Long status/error messages grow their panels, and bottom actions remain keyboard-scrollable.

Static generated lettering is reused from the boards. Native dynamic copy uses Windows Bahnschrift at sizes specified by the layout, with a 12-pixel floor. The generated boards do not identify an exact font. Multi-screen boards are normalized to the Home shell; their lower-resolution artwork is necessarily softer when enlarged. Windows dropdown/spinner and scrollbar affordances retain native behavior. The source chooser and expanded source rows have no generated reference and use the approved common shell; they are not claimed to match an absent board.

## Public v0.2.17 integration

The approved presentation below was integrated onto public v0.2.16. Release
packaging uses the verified v0.2.16 engine and payload; the historical private
v0.2.15 candidate described below is not used as the release game payload.
Public art metadata retains PNG hashes and generated/approved provenance;
private Library identities and reference archive/spec files are not exported.

## Build and validation

Normal release construction remains `codex/windows/single_exe/build-release.ps1`, now including the presentation resources and DPI manifest. For a private visual candidate that must preserve a release's exact game payload:

```powershell
./launcher/build-visual-candidate.ps1 -BaseExe <verified-release.exe> -BaseManifest <release-update.json> -Output <candidate-directory>
./launcher/test-headless.ps1
./launcher/test-updates.ps1
./launcher/test-desktop.ps1
```

The candidate builder verifies the base EXE's size/hash and the embedded payload's hash, then compiles only the launcher wrapper against that same payload. It writes `candidate-build.json` and does not publish, merge, activate an installed version, or change the release version. Existing version routing remains intact; use a separate empty installation folder to review a same-version candidate instead of replacing an existing installation's files.

The desktop harness uses a separate never-activated Windows desktop, synthetic installation/shortcut folders, and fake update transport. It exercises back/Escape, tab/focus, online navigation, update consent and failures, optional fields, cancellation callbacks, minimum-size scrolling, parent clipping, long recovery text, and simulated WM_DPICHANGED at 96/120/144/192 DPI. It records input-desktop isolation and screenshots. It never launches gameplay or changes network settings. Actual movement between monitors, real online gameplay, and real user source extraction were not performed.

Latest validation: 31 headless checks, 23 updater checks, and 116 UI assertions passed. The final private executable also passed `--verify-only` in a workspace fixture, with no gameplay/helper/network start. The v0.2.15 game payload SHA-256 remains `f60a8e1e9734ec7b3ec2584ad79e301b7b17c09da2401d9752df17a96170124c`.
