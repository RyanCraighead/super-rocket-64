# Configurable car speed — local candidate

The pinned backend uses Rocket League units and a proper axis rotation to host
units at scale2. Its maximum linear speed is2300RL/4600host units per second;
supersonic entry is2200RL/4400host. The host currently hardcodes additional
enemy, boss, Bob-omb and downward-impact thresholds. Merely clamping the final
velocity would strand those thresholds and leave acceleration/steering wrong.

Implemented policy: an integer speed percentage, range50–100, default75,
100 restoring the original behavior. Missing/invalid config becomes75; valid
explicit choices persist. No prior speed preference exists to migrate. One
shared policy header converts percentages to a multiplier. The engine's lower
level standalone physics API retains100 until configured, for explicit
compatibility; the game always supplies the saved/host rule before simulation.

| Call path | Treatment and reason |
| --- | --- |
| Car.cpp wheel throttle, brake and coast | Scale propulsion/brake force and evaluate torque/stopping curves at speed divided by multiplier. This preserves response relative to the selected top speed. |
| Car.cpp steering, powerslide and friction | Evaluate steering curves at normalized forward speed; keep geometric steering angles, slip ratios, surface grip and suspension unchanged. Scale the tiny linear slip cutoff, not material coefficients. |
| Ground/air boost and air throttle | Scale translational propulsion; preserve fuel consumption, coin capacity/refills, minimum boost time and cap allowance. |
| Car.cpp universal linear cap | Scale the existing linear cap in all axes, intentionally also lowering terminal speed on very long falls. Gravity acceleration and time remain unchanged. Ordinary jump velocities are below even the minimum cap. No extra post-frame-only clamp. |
| Jump/double jump and flip | Preserve vertical jump impulses/hold acceleration, flip rotation, damping, availability and timing. Scale horizontal dodge impulse and its speed normalization because these are translational propulsion; document the shorter horizontal travel. |
| Air roll and auto-righting | Preserve torque, angular caps, auto-righting impulse and all timers. These are orientation/recovery, not road speed. |
| Water jet | Scale driven forward and swim-up acceleration; preserve buoyancy, drag, gravity cancellation and native current. Metal sinking remains the native independent sink adapter; driven wheel/boost speed still follows the rule. |
| Native wind/current, moving platforms | Preserve level-authored accelerations, displacements and platform motion; the existing car linear cap still bounds resulting motion. Never scale gravity, world geometry or level timers. |
| Enemy supersonic damage | Use4400*multiplier host units/sec, including Bob-omb destroy-vs-bump classification; preserve native damage amounts and consequences. |
| King Bob-omb/Bowser bumper attacks | Use1800*multiplier, retaining rear-angle, native vulnerability, cooldown and geometry. |
| Ordinary Bob-omb bump | Use360*multiplier; native Bob-omb animation, fuse, knockback speed and reward stay authored. |
| Whomp/blue switch flip and dive | Scale linear descent thresholds120/1200 and linear minimum travel; preserve angular flip threshold, genuine physical witnesses, native windows and passive Whomp tire contact. |
| Player bumps | Real relative momentum naturally decreases with speed. Scale the authored impulse cap/linear eligibility deadbands; preserve masses, restitution and geometric penetration correction. Mario's native movement remains independent. |
| Camera follow | Normalize its speed-dependent distance and heading blend onset; preserve look input, collision distances, smoothing time and horizon. |
| Visual wheels and audio/HUD | Wheel spin already uses actual distance/radius and should stay so. Owned audio uses jump/flip/boost events with no RPM/speed thresholds. HUD shows fuel and physical controls, no speed threshold. Backend supersonic state must use normalized speed even though no new supersonic audiovisual effect is currently presented. |
| Native traversal/handoffs | Door/chimney/pipe parking/descent tolerances and warp distances are world-absolute contact constraints, not a fraction of top speed. Preserve them. Preserve native Mario action thresholds after handoff and physical velocity mirroring. |
| Validation limits and sweep envelopes | Keep finite/range/time/teleport and maximum-wire bounds conservative and world-absolute; do not reduce them into false packet/geometry failures at a setting transition. |

The implementation uses the existing verified-generation approach to adapt the exact pinned Car.cpp
sites in the build directory. Never modify or rehash the pinned vendor checkout.
A thread-local active-world query, already used for environment stepping,
provides the multiplier to the generated code. At100 each adaptation is an
identity operation. No dependency download or installation is needed.

Online extends the existing host-authenticated session rule and join payload.
Client menus show the effective rule and cannot overwrite it or their saved
offline preference. Owner pose records carry speed percentage/rule revision;
bump grants carry the revision. Stale/mismatched rule contacts are rejected,
and the compatibility suffix requires matching clients. Rule changes invalidate contact continuity without resetting fuel,
jump/flip state, health, saves or the physical body. Tests cover late joins,
duplicate and forged rules, changes while moving and old pose/grant delivery.

Verification completed locally:

- 9,609 actual RocketSim assertions at 50/75/100: acceleration/top speed, braking, steering/powerslide, air propulsion, water, fuel/caps, jumps/flips, gravity, terminal speed and setting changes without body/fuel/timer reset.
- A 1,920-frame trace across eight 100% scenarios matches the saved pre-change libraries byte for byte. Pinned vendor files remain unchanged and pass the 269-file hash check.
- 191 speed-boundary assertions cover enemy/Bob-omb/boss, Whomp and blue-switch dive/flip entries, player impulses and invalid bounds.
- 108 real rule/packet checks, 241 config/persistence checks, 1,014 codec checks, 206 real transport checks, 257 bump checks and nine pre-relay checks. Cases include forged rules, invalid ranges, late joins, duplicates, old sessions, A-B-A rule changes, stale owner poses and old pending grants.
- Actual 50% and 75% physics samples replay through the owned native collision/behavior functions: 16,423 assertions per setting, including three Whomp King hits and one star, small Whomp loot, eight headings, offline/host/client and blue-switch activation/rejection. The proprietary collision inputs and generated samples stay outside the repository.
- 3,389 camera checks include normalized distance at all three settings. Existing tree, boss, cap, door and pipe regression checks remain applicable; full results are recorded in the local release verification.

The standalone physics API and pre-existing component fixtures explicitly use 100% unless configured. The game supplies its saved/host value before stepping. This distinction is intentional; it does not bypass the game's 75% default.

The current game session was never used for testing. Headless native-function tests and process-local SDL input do not establish physical-controller feel or two-PC/WAN acceptance. This candidate remains local until publication approval.

The combined Windows engine and standalone EXE build successfully. The exact payload/nested-archive audit checks 1,867 files and retains 777 notice paths with no flagged proprietary inputs, credentials or private paths. Running the actual EXE in verification mode preserves all 132 copied data files (including saves and explicit speed/camera preferences), their timestamps and launcher preferences. Cached Mario/Octane assets and all 115 shared engine-data files verify without original source paths. This is a local candidate, not an uploaded release.
