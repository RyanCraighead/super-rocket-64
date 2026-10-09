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

## Pyramid poles

The two native pole-grab objects inside SSL area 2 now accept local airborne
car contact. The actual oriented car box supplies nose/side reach to these
pole cylinders, with native collision-list limits and solid-wall visibility;
other objects retain their existing reach. Native Mario retains the actual grab, turn, climb, slide, top and
jump-off actions. The configured steering stick and keyboard supply native
climb/turn input through a temporary controller copy; remapped jump uses a fresh
edge. Held entry, menus, focus, pause and freeze cannot synthesize a jump. Other
tree/pole contacts retain car control and native Mario remains unchanged.

While attached the complete car stands nose-up along the pole. Its offset
Octane box is not scaled: the 241-unit length fits vertically through the
authored 203-by-205 upper shaft. Native capsule floor/wall/ceiling checks still
run, with additional full-car swept position/yaw clearance from the actual
nearby static/dynamic surface cells. A blocked climb retains its last clear
position. The same transform rotates body and wheels in native presentation.

Jump-off hands the real native launch velocity and vertical orientation back
to RocketSim. It retains fuel, marks the native first jump as spent and leaves
normal aerial controls, fresh second jump/flip, boost and real body collisions
available. Held actions remain quarantined until release. The same pole cannot
immediately grab the car again before it leaves the native overlap region.
Object deletion, selection/area changes and native cancellation retire state.

The existing presentation/driving packet activity carries these poses; no new
wire fields or compatibility marker are needed. Remote observers never run a
second local grab. Tests exercise actual native actions, remaps and adapter
handoff, full-model presentation/codec, both owned pyramid climbs, all headings
through the narrow shaft, all three surface modes at 50/75/100% speed, real
mid-shaft launch collision and impaired packet delivery. These are headless
fixtures, not a rendered gameplay or physical-controller session.

## Castle light beam

Park upright on all four wheels on the native castle lobby light-beam floor
for two uninterrupted seconds to enter the Wing Cap tower. Release driving,
jump and boost controls. The native saved-star requirement still applies;
this does not unlock the cap switch or award stars. The existing F2 warp owns
the thirty-frame white transition and authored destination.

The car bridge samples the actual floor under the chassis after motion and
requires nearby ground, dry contact and only small resting velocity. Leaving,
moving, jumping, changing character/area, injury, pause, frozen control,
focus loss or menu input resets the wait. Duplicate frame calls do not add
time, skipped frames restart the wait, and an accepted visit fires once.
Native Mario's camera look-up path and all saved settings remain unchanged.

Windowless tests run the actual native warp scheduling/destination functions,
the real car adapter and controller/keyboard mapping, with explicit saved-star,
surface, runtime and audiovisual service fixtures. No real gameplay capture
or controller hardware session is implied.

## Camera cycle

Triangle on PlayStation / Y on Xbox cycles between the existing Car follow and
Mario views. Change it in Options > Controls > Car Controller > Cycle camera;
every supported button or trigger, deliberate shared bindings and Unbound are
available. Keyboard uses the existing saved Y action (default M), remappable
under Extra binds. One fresh press changes the view and saves the preference.
Held input across menus, pause, focus loss, native cutscenes, frozen control,
character changes and rebinding cannot queue a later toggle. The camera action
is local and never enters physics or network input.

The version-2 binding record appends the camera action. Existing version-1
records retain all eight actions, steering stick and inversion choices. They
receive Triangle/Y only if that button is unused; otherwise camera starts
Unbound so a saved driving action does not gain a surprise second action.
Version-2 camera choices, including Unbound and deliberate sharing, round-trip
unchanged. Existing camera mode and keyboard mappings are preserved.

## Scoped native tutorials

The existing courtyard Lakitu welcome now introduces Super Rocket64 with only
the driving, brake, steering, jump, boost, camera and options/rebinding basics.
BOB's first entry covers Goomba landings, supersonic impacts, double jumps and
flips, coin boost and surface grip. Every yellow, red or blue coin pickup adds
five boost, capped at 100; coin value does not multiply the grant. The text
shows the effective session boost and surface mode, including host rules.
Whomp's Fortress's entry and King Whomp's existing introduction explain the
exposed-back attacks: four supported wheels, a qualifying flip or a fast
boosted nose dive. Native small-Whomp loot and one boss hit per fall remain
unchanged; the tutorial does not hardcode boss health or award progress.

Prompts follow fresh input from the active mapped controller or keyboard/mouse.
They use saved car bindings, saved native pause/options keys, and the chosen
steering stick. PlayStation, Xbox and Nintendo labels follow SDL's device type;
unknown mapped pads use physical-position names. All text is rebuilt after a
remap. Reflow keeps the page number and waits for horizontal scrolling to finish.
Native font size, six-line pagination, confirmation, closing and cutscene
triggers are retained. Lua text overrides and replaced/custom dialog entries
remain authoritative. Classic modes without the car feature retain original
text. No ROM strings, save flags, physics or network data are changed.

`tests/test_tutorials.sh` uses the production SDL/keyboard and encoder, plus
verbatim native dialog state/pagination code. It covers all car binding choices,
device families, live rebinding, long labels, scope, closing and Lua precedence.
Owned background vertices verify a 143-pixel box with a 128-pixel text budget;
the fixture records native glyph positions. SVG/PNG evidence uses representative
glyphs and is explicitly a source-test render, not a running-game screenshot.

## Wing Cap on the car

Native Wing course entry and flying triple-jump handoffs return the selected
local car to ordinary Octane physics. The cap grants temporary unlimited boost
for the existing native timer; it adds no flight controller, lift or gravity
change. Mario and other characters retain their native action paths. Losing
the cap restores the existing finite balance and current saved/session boost
rule. Coins still affect the finite balance under the coin-only rule. Character
handoffs clear the temporary physics allowance and reacquire it only from a
still-valid cap; resets retain finite fuel.

The native cap geometry is centered on the measured cabin roof (33 source
units up, no forward displacement), using the full car basis through flips.
Sinking, native squash, cutscene presentation priority and cap expiry flicker
follow the car. Local cap transforms skip independent native interpolation so
they do not trail the custom renderer's current car pose. Existing remote cap
leases and packet formats are unchanged.

`tests/test_wing.sh`, `tests/test_wing_handoff.sh`, the host adapter suite and
the `rocket_wing_physics_test` target cover native course entry/timer expiry,
actor selection, arbitrary roof orientations, sinking/squash and actual Octane
ground/air/flip trajectories, finite fuel and boost-preference transitions.
Private owned-mesh projections are geometry diagnostics, not game screenshots.

## Downstairs support

Descent over connected static stairs uses one support plane for both tires and
chassis. The previous low-riser helper only handled upward clearance; descending
short treads repeatedly lost wheel support and struck flat floors/vertical faces.

The geometry proof requires at least three aligned risers, 4..60 host-unit
rises, 8..240-unit runs, and a fully covered horizontal tread with one material.
It subtracts actual floor triangles, so overlapping triangles cannot fill a gap.
The wedge above each tread must be empty: low obstacles, overhangs and missing
floors cannot be covered. No authored geometry or moving platform is rewritten.
Complex custom geometry fails closed to the original collision path.

Entry requires real wheel support and downhill velocity/intent. Ordinary ascent
keeps the existing low-riser handling. While descending, a supplementary bounded
chassis surface shares the tires' plane, and contacts with internal treads/risers
inside its proven empty wedge are suppressed. Other floors, walls, obstacles and
ledges remain authoritative. Stops, reversals and natural crest takeoff retain
coherent support while the car overlaps the flight. A requested jump can consume
its current support; the allowance ends when the real jump/flip starts, or on
water entry, leaving the flight, reset, recovery or geometry replacement. No
fuel, ability state, horizontal velocity, timer, packet or saved setting changes.
Native tread materials and all three surface modes continue to apply.

`rocket_stair_descent_test` compares measured descent with an equivalent actual
ramp at several grades, throttle levels, angles, speeds and surface modes. It
also covers gaps, overlapping geometry, material seams, low obstacles, walls,
variable frame stamps, repeated render calls, pause, stop/reverse, jumping,
reset/recovery and mesh removal. The optional `--routes` command verifies four
private owned castle-basement flights without bundling their geometry. Exact
parent/candidate ascent comparisons protect the accepted castle case. These
headless checks do not establish rendered gameplay feel.
