# Installed launcher: Characters instead of Setup

This is a bounded follow-up to `95a6cbf15bf4c585aca3caa19187a66fe3f19faa`. Keep that checkpoint intact and integrate this commit after it. Both are based on the cumulative v0.2.19 payload; no game payload, physics, networking, in-game settings, or Python helper changes are included.

Once the helper verifies the engine and Octane assets, the sidebar's Setup action becomes **Characters**. It and Play's **Add characters** action open the same standalone page. That page shows the Characters heading and optional character controls, with no Location / Games / Characters / Install / Ready stepper or wizard opt-in controls. Back returns to Play. Fresh or incomplete installations still receive the existing full wizard and its source validation.

Play's installed-state repair action is explicitly labeled **Repair installation**. It opens a separate recovery page offering Verify installation and Repair program files; merely opening it performs no operation. A successful verification of an already complete install now returns to Characters instead of reopening the game-source chooser. Failed or incomplete inspection retains the existing recovery/wizard path. Repair still honors game-running and atomic-operation guards.

The later user correction explicitly dropped the source-field prefill feature. No new persistence file, preference migration, guessed source path, or hidden-field prefill was added. Existing installation/source field values and stored preferences remain untouched by these navigation changes. The source audit found that the C# launcher stores update/shortcut settings, while the helper keeps a verified ROM copy and asset identities rather than the original selected filenames. The supported base ROM remains **Super Mario 64 US for N64**, not SM64 DS. Existing source validation and source reuse are unchanged.

Visual review also caught text-encoding artifacts in separator/caption strings. These are corrected without changing the referenced artwork. Explicit UTF-8 reads/writes are required when handling these C# sources on Windows.

Validation: **1,826 UI assertions**, **32 headless checks**, **13 source checks**, **3 source-fingerprint contracts**, and **23 updater checks** passed. The UI suite adds checks for fresh Setup versus installed Characters, matching sidebar/Play entry points, no installed wizard controls, 96/120/144/192 simulated DPI and 960x640 fit, Back, explicit repair without an automatic operation, actual synthetic verification returning to Characters, and unchanged installation/source fields and preference bytes. Existing lifecycle and updater tests remain passing.

All UI tests ran on a separate desktop that was never activated. Input stayed on Default; no test child became foreground. Synthetic process fixtures were used; no live game, production setup helper, user data, or online session was exercised. Real monitor transitions remain untested.

The private v0.2.19 candidate is 57,847,296 bytes, SHA-256 `c59afdc5166eced35586b2364a0e57b164202f4574546d7a60ad7cc253190c12`. It passed `--verify-only` with the exact unchanged v0.2.19 embedded payload, SHA-256 `e902886de54e4b57c3e2bea9c0bb8922ab5b1af08c0d7ec231951a04399df37c`. Release integration must use the next public version and latest cumulative payload; this task did not publish or merge main.
