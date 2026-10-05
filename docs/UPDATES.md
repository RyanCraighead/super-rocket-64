# Updates, shortcuts and removal

After setup, choose **Keep updated automatically** or **Manual updates**, plus your shortcuts. Start Menu is suggested; Desktop is optional. The choice remains changeable in **Settings**.

Automatic mode checks GitHub at startup, downloads and verifies a newer release, then applies it before play. Manual mode makes no startup update request; use **Check for updates** and **Download update and restart** when ready. Existing check-only permission never becomes automatic-install permission: the launcher asks for the new choice while retaining the old check-only behavior until you choose.

The launcher checks the latest 20 published releases of this public repository, including previews, and selects the highest supported numeric version. Drafts are ignored. It requires no GitHub account or private repository access. Offline, download and verification failures show an actionable message with retry and access to a verified installed game. There is no technical log panel. A failed release is paused for automatic retry; manual retry or a newer release remains available.

Before installing, the launcher validates the repository/tag-bound HTTPS URLs, GitHub SHA-256 asset digests, the update manifest, executable size/hash, release version, payload hash and supported data/update format. The downloaded EXE verifies its own embedded files in a windowless probe. This is hash verification against the official release, not an Authenticode signing claim.

Downloads use a separate temporary file. Cancellation or an incomplete download removes that file and retains the working version. Completed verified downloads can be reused. A per-install operation lock prevents concurrent setup/game/update actions. An active game blocks activation and rollback; close it normally. Updates never terminate a running game or replace the running launcher EXE.

Each launcher lives in its own `launcher-versions` directory. A small atomic state file selects the active and previous versions. The new launcher must complete a startup handshake before it can offer gameplay. A failed restart restores the previous selection. Rolling back switches updates to Manual so the removed release is not installed again automatically. **Roll back launcher** is available after an updater-capable version has been replaced; v0.1.0 and v0.1.1 predate this mechanism and remains downloadable separately.

Shortcuts target `<installation folder>\Super-Rocket-64.exe` with the explicit installation-folder argument. That persistent launcher routes to the selected version, so shortcuts do not depend on Downloads or a particular version directory. Setup does not overwrite a shortcut belonging to another installation. Keep using the same installation folder to retain your existing data.

## Remove shortcuts or uninstall while keeping data

**Remove launcher shortcuts** deletes only this installation's matching Desktop/Start Menu links. It leaves every program version, asset, save and controller setting intact. Unchecking a shortcut and saving preferences removes that shortcut too.

This portable release does not register a system-wide Windows uninstaller. To remove its program files, first remove its shortcuts and close the game and launcher. Keep or back up the installation's **`data` folder**, including `data\.runtime`; it contains the extracted assets, saves and configuration. You can then remove the `launcher-versions` folder, the versioned program folders, `Super-Rocket-64.exe`, and the `launcher-state.json`, `launcher-entry.json` and `launcher-preferences.json` files. Do not delete the entire installation folder if you want to keep `data`. A later installation can reuse that folder.

The updater does not alter firewall, VPN, router or security settings. Network play still supports Mario/Octane only and requires matching game builds.

## Setup and repair

The five-screen setup chooses location, validates required game sources, selects optional characters, shows progress, and finishes with play/update/shortcut choices. Verified profiles are reused; damaged characters can be rebuilt from the supported originals while their saves are retained. Cancel stops only the setup worker and discards its unfinished stage. Completed characters and verified tool caches survive retry.

**Repair program files** verifies a new embedded payload in a separate staging folder before swapping a damaged program directory. The old files are retained as a recovery copy. The stable `data` folder is never overwritten. Close a running game before repair. This does not broaden supported Rocket League versions or package hashes.
