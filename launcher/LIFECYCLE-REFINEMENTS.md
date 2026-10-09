# Launcher refinements after v0.2.19

This change is based on published v0.2.19, commit `db24fd83089d426d4988c8c2043455eb06618696`. The compact layout and exact source-selection validation are already in that baseline. Do not reapply the old design or compact-cleanup commits. Integrate this follow-up into the cumulative release branch and bump the public version there.

## Behavior

- Play keeps the Super Rocket 64 title and removes the main illustration and both slogans. Offline, Online, Add characters, and Setup / repair are direct actions. The installation-folder/car illustration remains.
- Add characters enters the optional-character chooser directly. It reuses verified core assets, preflights the selected optional sources, queues only selected optional characters, and returns to Play. A ready optional character can reuse its assets with a blank source. A missing optional source is requested on that page. It does not rewrite preferences, shortcuts, saves, or base source paths.
- New installations default to update notifications. Existing check opt-outs remain respected. Background startup checks do not block Play, install automatically, or show progress. The Play notification offers Update now and Later, once per version/session. Notifications defer during a running game or another operation. The normal update page contains only Download update and restart; Retry and Back are error-recovery actions. Preview-aware release selection is unchanged.
- Program extraction uses actual bytes written; program verification uses actual files verified. Update downloads use actual received bytes against the verified manifest size. These are phase totals, never an invented overall percentage. Helper import/extraction phases without a stable total and update verification/restart use an indeterminate travelling segment. Completion, cancellation, and failure clear the bar and busy state. Progress callbacks carry an operation generation and measured updates are throttled without fabricating intermediate values.
- Read-only installation inspection and preflight keep their current page with a short status. A game launch transitions to plain Game running after observing the installed game executable. The operation lease is released after that observation. Setup and another launch are guarded while the game is running.
- Closing the launcher window during launch or gameplay does not kill the game/helper. A lightweight foreground session thread continues draining redirected helper streams until the existing helper exits and performs its lock cleanup. Thus the window closes immediately, while its process may remain in the background for the session. No game or user process is forcibly closed.
- Closing during setup cancels at safe extraction/helper boundaries and then closes. Read-only steps finish/cancel safely. Update download/probe can cancel; atomic activation/restart finishes before closing. Close requests are nonmodal and never chain the next queued setup step. Repair waits for its existing atomic replacement to finish.

## Validation

Run from PowerShell:

```powershell
./launcher/test-headless.ps1
./launcher/test-updates.ps1
./launcher/test-desktop.ps1
```

Final results: **32 headless checks, 13 source checks, 3 source-fingerprint contracts, 23 updater checks, and 1,670 UI assertions**. Compilation treats warnings as errors. The UI harness creates a separate Windows desktop and never activates it. The input desktop stayed Default and no harness child became foreground.

Tests cover exact source validation, all pages at simulated 96/120/144/192 DPI and 960x640/1280x720, focus and Back/Escape, add-only preflight/install/completion, persisted preferences, default/legacy notification checks and opt-outs, deferred and dismissed notifications, the single-action update page, corrupt updates, measured progress and stale callbacks, canceled extraction with atomic cleanup/resume, setup success/failure/cancel/close, failed launch, close before/during a synthetic game, duplicate launch prevention, reopening, and update cancellation. Existing updater tests exercise real fixture-child verification and atomic restart/rollback with private-data sentinels.

The test-only child implements a synthetic process protocol. No production setup helper or game was launched; no live user sources, data, LAN session, or installed updater were exercised. Real monitor transitions and live gameplay remain untested. The supported Rocket League source fingerprints and compatibility limits are unchanged.

## Private candidate

The candidate was built with `launcher/build-visual-candidate.ps1` using the official v0.2.19 EXE and manifest. Both downloads matched the GitHub asset digests. Only C# launcher resources/code were rebuilt; the complete embedded game payload is byte-identical to v0.2.19.

- EXE: `Super-Rocket-64-v0.2.19-Visual-Candidate.exe`
- Size: 57,846,272 bytes
- SHA-256: `95e0fc6f7fc633189220cd0a369655b38fa783d964714bb75b249b00e14af167`
- Payload SHA-256: `e902886de54e4b57c3e2bea9c0bb8922ab5b1af08c0d7ec231951a04399df37c`
- Payload size: 48,241,483 bytes
- `--verify-only` succeeded in an isolated workspace destination; no game/helper/network was started.

This private candidate retains version 0.2.19 for inspection. It is not a new public release. Release integration must rebuild with the next version and the latest cumulative payload. No in-game menu, physics, networking, Python helper, or game assets were edited here.
