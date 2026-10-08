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

## Three surface modes

Settings shows Native: walls off, Native: walls on, then Octane. Missing or
malformed settings now default to Native: walls off (value 2). Stored 0 remains
Octane and stored 1 remains Native with wall driving on, so an upgrade never
reinterprets a deliberate choice. The same unsigned config field is retained.

Walls off adapts the verified suspension-ray decision before support, friction
and adhesion are calculated. An actual host-mesh hit must have an upward world
normal above the native floor/wall split (.01). Vertical walls and ceilings no
longer count as tire support. Real chassis collision remains, and the pinned
backend resumes gravity and air controls naturally. Rotated object-keyed faces
use their world normal. Slopes, partial ice grip, currents, poles/trees and the
bounded stair helper remain separate. Momentum and airborne boost are retained;
this option prevents tire-driven wall climbing, not rocket-powered flight.
Existing modes return the original support result and retain their trajectories.

The host's authenticated 16-byte session rule uses value 2 in the existing
surface byte. Pose packets stay 213 bytes. The compatibility suffix advances
env1 to env2: all peers must run this wall-policy version. Otherwise an older
client could join at value 0/1, reject a later value 2 update and silently keep
the wrong rule. Native join-request tests verify old-version rejection, while
rule tests cover host changes, late joins, stale/forged values, read-only client
widgets and restoration of the client's personal preference on leaving.

Real physics tests cover upward/reverse wall driving, support loss and gravity,
ceiling release, retained chassis collision, reverse exit, all air axes and
boost, plus bit-identical native floor/slippery/slope trajectories across static,
dynamic and rotated object-keyed meshes. The stair matrix also runs with walls
off, including every speed from 50 to 100; airborne goal crossings must land
on the real final tread without jump/flip grants. Owned castle and pyramid
geometry crossings pass at 50/75/100. The actual SDL/menu/config harness cycles
all three choices through controller and keyboard, preserves focus and reloads
each saved choice. All testing here remains windowless.

## Quicksand

Car ownership previously bypassed Mario's sinking update, then surrendered
controls as soon as any native sink depth exceeded one unit. The native bridge
now probes actual contacting tires and oriented chassis corners and calls the
original quicksand updater once per host frame. Shallow, ordinary, deep and
instant sand retain their native caps, hazard hook and fatal action. A stronger
pit contacted after motion can still trigger death on that landing frame.
Native death continues through its existing death hook, bubble or warp path.

Ordinary sinking retains steering, reverse and the configured jump binding.
A fresh jump press while buried starts the native thirteen-frame extraction
sequence, including its six depth reductions. Holding cannot repeat it or
produce a delayed jump when leaving sand. Pause/focus gates stop extraction,
and a new press below the native buried threshold can launch normally. Wheel
propulsion uses the native 6.25/depth factor; the initial grounded jump is halved
while sunk. Air controls, gravity, boost, global speed rules and collision
coordinates remain independent. The existing moving-sand current is preserved.

Sink depth is visual metadata: the renderer offsets a copy of body and all four
wheel transforms. Physical coordinates stay unchanged. Native presentation
already includes sinking and clears the metadata to prevent a second offset.
Four reserved bytes in the existing 213-byte pose packet carry bounded depth;
packet validation rejects nonfinite/out-of-range values and non-driving depth.
The native compatibility marker advances env2 to env3 so all peers agree.

Tests run the original sinking, extraction and fatal action functions, actual
adapter, every remappable controller jump binding plus keyboard, real physics,
native presentation, packet writer/ingress and render interpolation under loss
and reordering. Engine services in native tests remain explicit fixtures;
these checks do not claim a recorded game session or real network delivery.
