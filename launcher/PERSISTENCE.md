# Update and private-state persistence

Program packages are immutable and identified by payload hash. `Installer.Install`
creates or verifies their own version directory. Update downloads use a staged
file and verify its size/hash before promotion. `UpdateStore.Activate` switches
only the launcher version pointer; a game-start race or readiness failure restores
the previous pointer. Old program versions remain available for rollback.

Game data belongs to the stable installation's `data/.runtime` tree. Shared
bindings live at `controls/controls.cfg`; per-mode settings/save slots remain in
the selected profile's `save` directory. A program update does not migrate,
reinitialize, replace or delete those files. The launcher passes the same data
root to its next version. Controls are validated by the engine before launch;
invalid data is retained and reported instead of silently replaced by defaults.

`launcher/test-persistence.ps1` runs the production updater and package installer
against 28 synthetic private files covering seven offline/online save locations,
surface/speed/audio settings, named save-slot selection, remapped/shared controls,
backups, migration metadata, profile/material/model caches and update preferences.
It checks byte hashes and modification times through 11 transitions: old/new
package install, repeated verification, cancellation, corruption, download and
actual headless compatibility probe, early/late game guards, readiness failure,
successful activation and rollback. No real installation is read or changed.

The script's optional `-SourceRoot` compiles an explicitly selected source tree
without modifying it, allowing the current public launcher to be audited even
when a gameplay worktree has different launcher ancestry. It uses the selected
tree's update-transport fixture so its production interface is exercised exactly.

The companion `launcher/test-updates.ps1` covers official metadata, process
handshake/cancellation, locks and isolated shortcuts. The engine/launcher
`codex/windows/tests/test_shared_controls.py --engine <candidate.exe>` runs the
actual pre-SDL configuration parser, saved edits, migration and seven launch-path
choices without opening gameplay. These tests cover data retention and loading;
they are not an assertion that a new process takes over a currently running game.
