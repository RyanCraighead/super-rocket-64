# Updates, shortcuts and removal

The first launch asks whether to check for updates on startup. **Automatic checks are off until you opt in.** Start Menu is selected as the suggested shortcut; Desktop is optional. Choose **Save preferences** to apply your choices, or Back to continue without saving them.

Open **Updates & settings** at any time to change those choices, check manually, remove shortcuts or roll back to the previous updater-capable launcher. An enabled startup check displays the newer version and download size. Choose **Download update and restart**, **Later**, or **Don't tell me again**. The last option disables automatic checks; manual checks remain available. Updates never download or install without confirmation.

The launcher checks the latest 20 published releases of this public GitHub repository, including previews, and selects the highest supported numeric `vMAJOR.MINOR.PATCH` version. Drafts and unsupported tags are ignored. It does not require a GitHub account or access to a private repository. Offline, rate-limit and download errors leave the current installation available and are shown in the launcher log. Retry through the manual check.

Before installing, the launcher validates the repository/tag-bound HTTPS URLs, GitHub SHA-256 asset digests, the update manifest, executable size/hash, release version, payload hash and supported data/update format. The downloaded EXE verifies its own embedded files in a windowless probe. This is hash verification against the official release, not an Authenticode signing claim.

Downloads use a separate temporary file. Cancellation or an incomplete download removes that file and retains the working version. Completed verified downloads can be reused. A per-install operation lock prevents concurrent setup/game/update actions. An active game blocks activation and rollback; close it normally. Updates never terminate a running game or replace the running launcher EXE.

Each launcher lives in its own `launcher-versions` directory. A small atomic state file selects the active and previous versions. The new launcher must complete a startup handshake before it can offer gameplay. A failed restart restores the previous selection. **Roll back launcher** is available after an updater-capable version has been replaced; v0.1.0 predates this mechanism and remains downloadable separately.

Shortcuts target `<installation folder>\Super-Rocket-64.exe` with the explicit installation-folder argument. That persistent launcher routes to the selected version, so shortcuts do not depend on Downloads or a particular version directory. Setup does not overwrite a shortcut belonging to another installation. Keep using the same installation folder to retain your existing data.

## Remove shortcuts or uninstall while keeping data

**Remove launcher shortcuts** deletes only this installation's matching Desktop/Start Menu links. It leaves every program version, asset, save and controller setting intact. Unchecking a shortcut and saving preferences removes that shortcut too.

This portable release does not register a system-wide Windows uninstaller. To remove its program files, first remove its shortcuts and close the game and launcher. Keep or back up the installation's **`data` folder**, including `data\.runtime`; it contains the extracted assets, saves and configuration. You can then remove the `launcher-versions` folder, the versioned program folders, `Super-Rocket-64.exe`, and the `launcher-state.json`, `launcher-entry.json` and `launcher-preferences.json` files. Do not delete the entire installation folder if you want to keep `data`. A later installation can reuse that folder.

The updater does not alter firewall, VPN, router or security settings. Network play still supports Mario/Octane only and requires matching game builds.
