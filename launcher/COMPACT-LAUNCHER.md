# Compact launcher and validated source selection

This refinement starts from cumulative v0.2.18 (`548dd800fc1959150602b1706a3433705be10afe`). It changes only launcher UI, read-only source selection checks, and associated build/test wiring. The embedded game payload, gameplay, in-game settings, save/data contract and updater activation protocol are unchanged.

## Requested presentation

- Play retains the offline, online and setup/repair/add-characters actions and approved hero artwork. The character-wheel note and session-mute control are removed from that screen.
- Installation keeps the folder/car artwork and moves the folder input, Browse and subsequent actions upward. The explanatory heading, storage cards and asset/save/control copy are removed.
- Source selection shows two source fields and concise per-field feedback. Verification/cache/tool implementation explanations and the global source-location footer are removed.
- Pages fit the available width **and height** without scrollbars. The common shell retains its own scale so the title bar and footer remain anchored on wide windows. All five optional-character source rows fit together instead of growing a scrolling list.

## Source validation and repair

Next is disabled until **both selected sources pass**. `SourceValidation.cs` reads files without extraction, downloads, process launches or mutation. It normalizes the supported N64 byte orders, accepts a bounded single-ROM ZIP, checks the complete SM64 US SHA-1 and size, and checks both Rocket League package SHA-256 values and size limits. The fingerprints match the existing packaged Python extraction validators; `test-source-contract.ps1` checks that contract.

Changing either path disables Next immediately. Background checks are debounced and cancellable; stale results cannot enable a different selection. Clicking Next hashes again, then invokes the existing preflight. Back cancels a pending advance. The existing helper remains authoritative at extraction time.

The source-selection wizard requires explicit valid ROM and Rocket League selections even when repairing or adding characters to a cached installation. A blank field does not bypass this gate. The downstream installer still reuses valid completed assets. Users with a verified installation can go directly to Play without entering source selection.

Startup now routes verified engine+Octane installations to Play, including those without a saved update mode. Missing/incomplete installations enter Setup or actionable recovery. Existing explicit automatic-update preferences retain their update flow. Old check-only consent is retained on disk and never escalated into automatic installation; no forced Settings/Ready detour occurs at startup. Explicit update choices remain available in Settings and at the end of a new setup.

## Validation and candidate

- 1,575 UI assertions passed on a separate Windows desktop that never became active; the test child never became foreground.
- 129 native `PrintWindow` screenshots cover the affected screens and all launcher pages at minimum/default/short-wide geometry and simulated 96/120/144/192 DPI, including all five optional sources, long errors, source validation disabled/enabled, focus, Back/Escape and cancellation.
- 31 existing headless checks, 13 new read-only source checks, 3 fingerprint contract comparisons, and 23 updater checks passed.
- Source acceptance fixtures use **synthetic bytes and a separate test validator instance**. Production always uses the pinned supported-game fingerprints; the tests verify production rejects those synthetic files. No real user ROM or Rocket League package was read for testing.
- The warning-as-error candidate passed `--verify-only` in an isolated workspace installation. It retains the exact v0.2.18 payload SHA-256 `ca1856447d2a99467636de44eb56bdf64e1f0fd9ad2fda1a2a28ea4f9d968233`.

Private candidate: `Super-Rocket-64-v0.2.18-Visual-Candidate.exe`, 57,828,352 bytes, SHA-256 `49c2326f0a2dc87b11972768471e9e301bc06d8bd2d4466a8b8f4b19efaade78`. This is integration evidence, not a new public version. Rebuild from the release task's cumulative source after assigning the next version. No main merge or public release was performed here.

Real monitor transitions, gameplay and extraction from real user game sources were not exercised. Unsupported Rocket League package versions remain unsupported; their actionable message leaves Next disabled.
