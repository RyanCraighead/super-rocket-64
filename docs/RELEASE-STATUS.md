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

See [content and attribution](CONTENT-AUDIT.md), [supported inputs](ASSET-SOURCES.md)
and [building from source](../BUILDING.md). Matching release hashes are provided
with each release; do not assume an older development installer is identical.
