# Car ram and native Bully behavior

A car mirrors idle/freefall into Mario, so the native Bully handler used its
incoming-hit branch even when the car drove into the Bully. Both actors entered
knockback. A local car now qualifies for the existing fast-attack branch only
with current mapped positive throttle, grounded upright contact, at least 180
host units/second of forward motion, frontal contact within 45 degrees, and at
least 120 units/second of relative closing speed. Sideways slides, stationary
throttle, rear contact and an approaching enemy alone do not qualify.

The native interaction handler still owns attack status, knockback, collision
consumption, ownership callbacks, lava death, minion accounting and star/coin
rewards. This adds no reward, save field, packet, physics tuning or invulnerability.
Metal and ordinary Mario attacks retain their native behavior.

`bash codex/rocketleague/tests/test_bully.sh` compiles the production predicate,
interaction handler, Bully behavior and native object stepping/death code. It
passes 155 checks including analog/digital remaps, relative movement, rejected
contacts, local offline/server/client authority, Big Bully ring-out/star/bridge,
small Bully coins and minion counters. The ring is explicit flat test geometry;
this is source integration evidence, not a recorded gameplay session. UBSan is
enabled. `test_host.sh` checks the real adapter input gates, and
`test_incoming.sh` retains native damage/collision regressions.
