# Verification and limitations

This first Windows release keeps Octane as the default, the character wheel,
existing gameplay/progression fixes and direct-IP Mario/Octane multiplayer.
The assistant integration is removed. The updater and shortcut work are
reserved for a later release.

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
Extraction was tested with the verified Steam package pair; a separately
installed Epic copy remains untested locally. Matching Epic package support
must not be read as acceptance of every current/future Epic game update.

Public-source research also confirms the shared package layout in the
[VelocityRL author's source](https://github.com/bitsfdb/VelocityRL/blob/61e96ff0a6e0047f75fe73d72effc793c48aecce/src-tauri/src/integrity.rs).
This corroborates folder/container structure, not byte-for-byte identity of
uninspected Epic packages. No third-party implementation or game data was copied.

See [content and attribution](CONTENT-AUDIT.md), [supported inputs](ASSET-SOURCES.md)
and [building from source](../BUILDING.md). Matching release hashes are provided
with each release; do not assume an older development installer is identical.
