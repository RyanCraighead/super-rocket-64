# Local player-bump candidate

This branch is not published. v0.2.8 remains the public release.

Character sessions previously forced Player interaction to None and remote
Octane poses were intangible presentation. The candidate respects the host's
existing None/Solid/PvP setting. Native Mario/Mario interactions remain native;
car pairs use non-damaging physical bumps, never native stomps or PvP damage.
The default host setting is Solid. Only Mario and Octane are supported online.

The lowest connected global ID in the area (the host when present) evaluates
accepted owner poses. Car bodies use the pinned Octane box; Mario uses its
native cylinder. Contacts include a bounded translation sweep, require a clear
world path, and reject stale, cross-area, intangible, dead, frozen, cutscene,
warp and verified Vanish states. Stationary/no-input cars still have mass.
Solid character sessions send owner state at up to 15 Hz instead of the prior
7.5 Hz, including stationary players. Very stale contacts fail closed.

Reliable, bounded velocity grants affect only each recipient's own simulation.
The arbiter waits for both acknowledgements and newer owner samples before
issuing another impulse for the same pair. Owner history, epoch, area tokens,
native lifecycle gates and event replay windows reject delayed/repeated hits.
The server validates the physical sender and area authority before relaying.
There is no remote control input, position teleport, health damage, demolition,
boost refill, ability reset or collision removal. Ordinary owner pose updates
reconcile the resulting movement; this is not a global rollback simulation.

The local protocol suffix adds `bump1`; all players must use the same candidate.
Existing public builds are not silently allowed to mix with this protocol.

Verification: 235 authority/contact/packet fixture assertions, 9 actual native
pre-relay ingress checks, and 548 actual RocketSim physics assertions passed.
These cover local host/client ownership, authority migration, stationary/fast
contacts, 0/50/100 ms delivery delays, duplicate/reordered acknowledgements,
epoch/area/selection changes, reconnect history, stale/invalid/forged packets,
world-path rejection, momentum and preservation of fuel/jump/flip state.
Existing codec (1,009), transport (191) and online wheel-switch regressions pass.
Native movement/transport boundaries in the authority fixture are explicit
services. Live gameplay, visual feel, real network jitter and two-PC acceptance
remain unverified. The combined local Windows build is still pending.


Combined candidate update: the Windows engine including this change and the configurable speed rule builds successfully. Publication and live gameplay acceptance remain pending; no active game was interrupted. See [car speed verification](LOCAL-CAR-SPEED.md).
