# Difficulty presets

Choose **Options > Octane difficulty**:

| Preset | Speed | Jump height |
| --- | ---: | ---: |
| Easy | 100% | 100% |
| Medium (default) | 75% | 50% |
| Hard | 50% | 30% |

The speed and jump sliders remain available. **Custom** appears whenever their values do not match one of these pairs. Use the sliders to make a custom setup; selecting Custom itself leaves the current values alone. Difficulty changes only these two settings. It does not change enemies, health, timers, boost/surface modes, gravity, flip timing, swim-up or other characters.

Existing saved custom values survive upgrades. The label is derived from the two numbers, so there is no separate preset preference that can overwrite them. A missing/invalid individual value falls back to its established default: speed 75%, jump 50%. A new installation therefore starts at Medium. Selecting a preset commits both values in one staged config-file replacement and one host-rule update; a failed save keeps the previous pair and reports the failure in the options panel.

Online, the host chooses the shared preset or custom values. Clients see that effective pair and cannot change it. Joining does not overwrite their saved offline choices. Late joins receive both values in the same authenticated rule. All players need the matching `tune2` build: older releases reject heights below 50%, so mixed sessions are rejected at join.

Hard's single jump is approximately 30% of original rise, not 30% impulse. The actual pinned physics measures 46.94 host units for a short tap and 126.42 for a full hold, versus 155.25 and 429.53 at 100%. The existing 50-100% curve is unchanged. Double-jump totals depend on input timing and momentum; the tested Hard sequences reach about 26% of the original total height. [Jump measurements](CAR-JUMP-HEIGHT.md).

Hard intentionally reduces unboosted reach. A route may need a held jump, second jump, boost or a higher manual setting. Native boss/button contact replays check eligibility and consequences at 50% speed / 30% jump; they do not prove every route can be completed with that preset. Full-playthrough, physical-controller feel and two-PC/WAN acceptance remain unverified.

Developer checks cover actual preset UI callbacks and manual edits, atomic save/rollback, old custom config loading, missing/invalid values, host authority, late joins and stale packet rejection. Physics checks cover every integer height from 30 to 100 at 50/75/100 speed and seven hold durations; 100% original snapshots compare exactly. The engine's `--verify-difficulty SAVE_DIR [PRESET]` entrypoint loads the real config without opening a window and optionally applies preset 0/1/2. It writes config/backup files; use isolated fixture directories only.
