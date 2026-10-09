# Car ram and native Bully behavior

A car mirrors idle/freefall into Mario, so the native Bully handler used its
incoming-hit branch even when the car drove into the Bully. Both actors entered
knockback. A local car now qualifies for the existing fast-attack branch only
with current mapped positive throttle, grounded upright contact, at least 180
host units/second of forward motion, frontal contact within 45 degrees, and at
least 120 units/second of relative closing speed. Sideways slides, stationary
throttle, rear contact and an approaching enemy alone do not qualify.

The first implementation only classified the first frame. Native knockback then
lowered relative closing speed while the car's longer chassis still overlapped
the Bully, so the next frame took the incoming-stun branch. Mario's capsule push
also failed to separate the longer car, allowing repeated consequences.

Each local car/Bully pair now records one native consequence for a continuous
contact. A measured gap of 64 host units (one bounded correction step) rearms it;
small gaps produced by resolution do not create a rapid new impact. The collision
prepass can follow the displayed car pose through native injury. Missing poses
do not count as separation. Allocation, area, level, character, sync identity and
death transitions discard stale pairs. Consumed contacts return without stopping
the native dispatcher from processing a different hazard. Stationary incoming
hits still hurt, and immunity/delayed contacts are not consumed prematurely.

A successful ram preserves the car pose and native Bully knockback. It uses the
existing bounded horizontal velocity impulse to prevent the car overtaking the
Bully, retaining tangent/vertical velocity, fuel, rotation and suspension. Native
Bully stepping resolves residual horizontal penetration in at most 64 host units,
using steps no larger than 8 units checked against walls and floors. It never
lifts the Bully, jumps a wall/corner, or bypasses native floor/lava stepping. A
blocked path also stops inward car velocity. Above-roof falls remain incoming
contacts. Only the native object owner can apply outgoing movement corrections.

The native interaction handler still owns attack status, knockback, ownership
callbacks, lava death, minion accounting and star/coin rewards. This adds no save
field, packet or global invulnerability. Ordinary Mario keeps its capsule push.
Metal attacks retain native attack and reward behavior with car-sized separation.

`bash codex/rocketleague/tests/test_bully.sh` compiles the production predicate,
interaction handler, Bully behavior and native object stepping/death code. It
checks analog/digital remaps, relative movement, 300-frame sustained overlap,
fresh re-entry, native injury presentation, independent hazards, high-speed
native update order, rotated contacts, wall/corner/floor limits, timer wrap,
offline/server/client authority, and Big/small/minion ring-out rewards. Native
object movement and wall reflection execute against explicit flat/plane fixture
geometry. This is source integration evidence, not a recorded gameplay session.
UBSan is enabled. `test_contacts.sh` includes the real ledger, collision prepass
and pool allocator. `test_host.sh` checks real adapter input gates, while
`test_incoming.sh` retains native damage/collision regressions.
