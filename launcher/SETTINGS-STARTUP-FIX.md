# Launcher settings and startup fix

Historical v0.2.18 fix record. The subsequent [compact launcher refinement](COMPACT-LAUNCHER.md) supersedes the startup destination for verified installations and replaces scrolling layouts with pages that fit the viewport.

Based on release v0.2.17 (`866600e93ad59387fac5602a05b20cebec8571ad`). Only the C# launcher presentation/startup and its desktop harness change. Game code, payload, data contract, save paths and update activation protocol remain unchanged.

## Reported rendering fault

Windows native window rendering reproduced the user's black backgrounds behind both update radio buttons and the shortcut checkboxes. `DrawToBitmap` had hidden this fault by compositing transparent children differently. The custom choice painter now paints the parent canvas into its own drawing buffer before drawing the real radio/checkbox and text. Settings descriptions no longer overlap their radio control rectangles. The same painter fixes the corresponding Setup controls.

The desktop harness now captures windows through `PrintWindow`. It checks the choice background pixels and non-overlapping descriptions in both selection states at minimum/default sizes and simulated 96/120/144/192 DPI. Keyboard focus, Escape, scrolling and neighboring pages remain exercised.

## Startup routing

| Installation state | First destination |
| --- | --- |
| No data directory, even with saved update preferences | Setup, Location |
| Failed or incomplete verification | Setup or actionable recovery |
| Verified engine and Octane, no explicit update mode | Setup, Ready step to finish choices |
| Verified installation and saved manual mode | Play |
| Verified installation and saved automatic mode | Existing update check/apply flow |

The data directory and a saved preference alone never imply readiness. The existing `wizard-status` inspection remains authoritative; Play additionally requires consistent engine/Octane report fields. Readiness is cleared before another inspection so a failure cannot retain a previous folder's ready state. Old check-only consent is retained on disk and never converted into automatic installation consent. No preferences or shortcuts are rewritten until an explicit save/Setup completion.

Startup UI tests feed representative helper reports into the same completion callback used by real inspection; the missing-payload fixture separately exercises actual inspection failure and recovery. Existing Python inspection tests exercise the underlying validators without game extraction.

## Validation

- `launcher/test-headless.ps1`: 31 passed.
- `launcher/test-updates.ps1`: 23 passed, including real test-child update probe and restart handshake.
- `launcher/test-desktop.ps1`: 288 assertions passed; native captures on a separate desktop that was never activated, with no test window becoming foreground.
- `python -m unittest discover -s codex/windows/tests -p test_wizard_setup.py`: 7 passed.
- Private candidate compiled warning-as-error and passed `--verify-only` in a disposable workspace root. No game, helper or network request occurred in that verification.

Payload SHA-256: `52e10700c3b9070c05ed588f7e0a1fae5b11a1565235ea0d5f4dfdac4c964055` (exact published v0.2.17 payload). Candidate executable SHA-256: `1cd468dadad8d419c41373912a30fd59296c44bb8a3b525a7834a5f00ea19132`, 57,816,576 bytes.

Real monitor transitions, gameplay and real user asset extraction were not run. This candidate remains version 0.2.17 for private evidence; the release task must integrate the commit into its cumulative source and assign the next release version. No public release or main merge was performed here.
