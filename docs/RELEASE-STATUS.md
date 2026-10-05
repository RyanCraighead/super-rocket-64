# Verification and limitations

This first Windows release keeps Octane as the default, the character wheel,
existing gameplay/progression fixes and direct-IP Mario/Octane multiplayer.
The assistant integration is removed. v0.2.0 adds opt-in update checks,
confirmed installation/rollback and optional persistent launcher shortcuts.

The preserved baseline passed 71 setup/repair/cancel/integrity/launch checks,
64 actual launcher UI checks on a separate never-activated desktop and six
upgrade checks against a copied profile. The user also reported successful
manual play of the earlier v5 baseline. Those are development-machine results;
they do not establish all-character, physical-controller or two-PC/WAN coverage.
Optional characters are offline only.

The v0.1.0 release was rebuilt from the public source export. Its standalone
EXE passed windowless installation and complete payload hash verification.
Fresh setup from the owned SM64 and supported Rocket League inputs passed,
including all 115 shared runtime files and the engine's headless validation.
Reusing the setup without source paths preserved profile bytes, timestamps,
the test save, shared assets and the verified extraction-tool cache. The
complete payload and nested runtime/source archives passed the private-data
and notice scan. These final checks did not enter gameplay or use the desktop.

The public edition is exported from an explicit source allowlist without
private Git history. The ROM, Rocket League packages, extracted character
profiles and saves remain local inputs. Setup reconstructs shared game data
from the supported owned ROM and declared original/source-built components.

## v0.1.1 setup update

Epic Games Store installation discovery reads completed local Epic manifests.
Setup accepts Epic or Steam installations whose two required package hashes
match the supported profile, and rejects missing/unsupported packages before
provisioning extraction tools. No Steam client is required. The game engine,
converted geometry checks and multiplayer protocol are unchanged.

The new launcher passed 36 headless checks, 48 UI checks on a never-activated
desktop and five extraction/preflight checks. The rebuilt EXE passed payload
verification, clean owned-input setup, all 115 shared-file checks, headless
engine validation and source-free reuse preserving profile/save/cache bytes
and timestamps. The payload and nested archives passed the private-data scan.
Automated extraction was tested with the verified Steam package pair.
On October 5, 2026 at 01:31 UTC, Ryan reported: "I tested the epic installation
and it worked." Epic installation is now user-verified. This report adds
manual user evidence; it does not extend the automated test coverage or
establish compatibility with every future game update. Two-PC/WAN and new
physical-controller acceptance remain pending.

Public-source research also confirms the shared package layout in the
[VelocityRL author's source](https://github.com/bitsfdb/VelocityRL/blob/61e96ff0a6e0047f75fe73d72effc793c48aecce/src-tauri/src/integrity.rs).
This corroborates folder/container structure, not byte-for-byte identity of
uninspected Epic packages. No third-party implementation or game data was copied.

See [content and attribution](CONTENT-AUDIT.md), [supported inputs](ASSET-SOURCES.md)
and [building from source](../BUILDING.md). Matching release hashes are provided
with each release; do not assume an older development installer is identical.

The v0.2.0 launcher passed 36 baseline checks, 22 updater/shortcut checks and
85 actual UI checks on a never-activated desktop. Checks cover offline/no-release
responses, manifest/version mismatches, partial downloads, cancellation, actual
child probes/restart handshakes, failed restart rollback, operation locking and
actual shortcut target/argument verification. The game engine is unchanged.

The final v0.2.0 EXE passed a live official GitHub HTTPS check, its embedded
update probe and an upgrade/reuse test against copied v0.1.1 data. All 130 data
files retained their bytes and timestamps, including assets, saves and tool
cache. Its actual restart UI was exercised on a never-activated desktop:
game controls stayed disabled until the matching commit signal, then enabled;
the launcher closed normally. The 1,864-entry payload/nested archive audit had
no private-data or prohibited-input findings. These checks did not launch a game
or alter real Desktop/Start Menu shortcuts, networking or security settings.

## v0.2.1 folder picker simplification

Rocket League setup now uses one Browse folder picker for Epic Games Store and
Steam. The separate Epic discovery button and unused manifest code are removed;
package checks and extraction are unchanged. Existing updater, shortcuts,
assets, saves and configuration remain supported.

The patch passed 31 headless checks, 22 updater/shortcut checks and 95 UI checks
on a never-activated desktop, including both store folder layouts and Back
navigation. The actual packaged EXE passed integrity/probe/restart checks and
live GitHub HTTPS. Reusing copied v0.2.0 data preserved all 130 data files and
launcher preferences. The 1,864-entry payload audit had no findings. Epic remains
user-verified; two-PC/WAN and new physical-controller acceptance remain pending.
