# Octane jump height

Options > Octane jump height (%) accepts integer values from 50 to 100 and defaults to 50. Missing or invalid saved values become 50. A valid saved choice survives upgrades. Speed remains a separate setting, default 75. The standalone physics API starts at 100 for compatibility; the game explicitly supplies the saved or authenticated host height before stepping or exposing physical contact state.

The setting changes Octane's initial jump impulse, held-jump lift and neutral second-jump impulse. It does not change world gravity, the original minimum/maximum hold window, flip torque/timing/damping, horizontal dodge impulse, boost force/fuel, swimming lift, native Metal sinking, Mario or the other characters. Speed-based damage/impact thresholds still depend only on speed. A setting change never rewrites the body's position/velocity, replenishes fuel, resets jump/flip availability or modifies a save.

Height is measured as the car body's rise above its settled ground position. Halving impulse would approximately quarter a simple ballistic jump. This implementation starts with the square root of the desired height and applies small calibrated corrections to initial and held lift for the pinned 120 Hz suspension/sticky launch and original hold window. All factors are exactly one at 100%. The exact pinned vendor sources remain unchanged; the existing checked generator adapts three explicitly matched jump sites.

| Input, flat stationary ground | 50% height | 100% height |
| --- | ---: | ---: |
| One 30 Hz frame tap | 77.77 host units | 155.25 host units |
| Six-frame/full hold | 213.06 host units | 429.53 host units |
| Tap, neutral second jump on frame 35 after first on frame 30 | 223.16 host units | 474.58 host units |
| Full hold, neutral second jump on frame 40 after first on frame 30 | 406.00 host units | 869.60 host units |

The default single jump measures 49.60–50.30% of normal across all seven tested hold lengths. Across every integer setting and hold length, the largest deviation from the requested single-jump height ratio is 1.21 percentage points. Actual 50/75/100 speed comparisons produce identical vertical single-jump heights. Slopes of rise/run ±0.20 retain roughly half the surface-relative jump rise. A double jump is an added impulse at the current position and velocity; at the same tested input times its total rise is about 47%, not an exact half. Momentum, orientation, moving surfaces, second-jump timing and boost can change the absolute trajectory. The setting is not a world-height clamp.

Online uses the authenticated host session/revision, carried in the 16-byte rule and join payload. Clients cannot change it or overwrite their offline choice. Height changes advance the shared revision even when speed stays unchanged, so old owner poses and pending grants cannot cross a change or an A-B-A transition. The distinct `tune1` compatibility suffix requires matching clients. Rule application preserves physical state and retires contact continuity; it does not retroactively adjust a jump already in flight.

Verification includes 250,020 actual RocketSim assertions, all integer settings, tap/held/second jumps, directional flips, slopes, unchanged airborne boost and gravity, underwater jets, temporary boost allowance, Metal sinking and state-preserving setting changes. The no-jump and pure airborne-flip paths compare complete snapshots exactly. Config parsing/serialization, host authority, late joins, invalid/forged/stale rules and old pose rejection use the production functions. Existing speed-impact and gameplay regressions remain applicable.

Reduced unboosted reach is intentional: a ledge reached with a full-height tap may need a hold, second jump, boost or 100% height. The owned-geometry King/button replays validate native contact eligibility and consequences, not a complete route through every level. No full playthrough, physical-controller feel or two-PC/WAN acceptance was performed. The active game was never used for these tests.
