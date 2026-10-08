# Native terrain control checkpoints

## Slippery slopes

The host already derives slippery classes and slope cutoffs from native Mario
surface code. Flat slippery surfaces used reduced grip, but crossing a native
slope cutoff set all tire force to zero. That disabled steering/braking except
on the separately tagged CCM race slopes.

Slippery slopes now retain the existing class coefficients: half grip for
slippery, quarter grip for very slippery/ice. Those ratios come from the native
neutral slide losses (.04/.08 and .02/.08); no new slope threshold is added.
Actual wheel triangles select the coefficient, so mixed floors and dynamic
layers retain per-wheel behavior. The existing powerslide input still acts.
Ordinary steep floors retain their restriction; explicit Car Grip, normal
floors, chassis collisions, currents, jump/boost and the CCM race coefficient
are unchanged. No packet, save field or vendor source changes.

The regression compiles the actual physics adapter and pinned RocketSim. Before
the fix, a sliding slippery slope produced no steering displacement and nearly
identical coasting/braking speed. After the fix, steering and braking remain
usable but weaker than full grip, for slippery/ice/race tags and both mesh layers.
`rocket_environment_test` passes 8,525 checks, including original exact Car Grip
trajectories, mixed tires, mode/material changes, reset/pause and native currents.
Core/host-math physics, platforms, speed/impact, jump height, water/escape, Whomp
flips and player-bump physics suites also pass. These are windowless source and
physics tests, not recorded gameplay or a claim of original Rocket League parity.

## Low stairs

Native Mario follows the floor over low risers. The rigid Octane nose and its
suspension rays could instead catch a vertical face, leaving the car stopped at
ordinary castle and pyramid steps. The host bridge now checks the actual next
tread before each 120 Hz step while driving, dry and upright, with at least two
real supporting tires and no active/spent jump or flip.

The rise limit is the native upper ground-wall probe's 60 host units. Flat treads
use the native smallest slope qualifier, cos(5 degrees). Both upward and forward
convex sweeps use the full offset Octane box; a low ceiling or wall still rejects
the move. The existing collision margin supplies the short lookahead needed
when a driven car is already stopped against a riser. No horizontal translation,
extra engine force, smaller collision shape or replacement level ramps are used.

Tire forecasts target the transformed pinned Octane resting plane, so repeated
predictions do not repeatedly add the same rise. If a tilted suspension ray
already strikes a riser, its own neighboring floors and actual wall-contact
height determine the small clearance needed. Using another axle's higher floor
or the nominal tire plane alone misses a rear wheel on short stair flights.
An accepted supporting step resolves downward motion; upward/horizontal motion,
fuel, ability expenditure, timers, native surfaces and wire formats are retained.

The real-physics test covers 25/26/51-unit risers, short/long treads, 50/75/100%
fixed-input driving, reverse/diagonal motion, both mesh layers and object-keyed
platforms. A separate fixture driver uses explicit steering feedback to verify
every saved speed from 50 through 100 on a long flight. All 40,686 checks pass.
Ten non-stair trajectories are exactly equal to the prior adapter: flat floor,
tall walls, unsupported riser, low ceiling, ramp, blocked input, air motion,
flips and descent. This preserves existing wall handling; the separately queued
three surface modes are not part of this stair change.

Private integration fixtures load the owned game's actual castle/pyramid
collision triangles. They demonstrate old-build stops and candidate crossings
at the same coordinates and inputs. Those decoded assets remain outside source
control. The executable's optional --geometry path reads a caller-provided
triangle fixture; no game process, renderer, player input or installed save is
used. The tests do not substitute for post-release rendered gameplay acceptance.
