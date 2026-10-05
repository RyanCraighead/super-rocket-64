# Local tree-contact fix

The supplied screenshot shows Octane suspended in a tree canopy. A still image
cannot reveal the native action. A headless replay of the actual native hitbox,
pole handler and car adapter confirmed an actionable cause: airborne Octane
entered ACT_GRAB_POLE_SLOW, which suspended car physics, and native pole
positioning then pinned the player to the trunk.

Cars now reject native tree/pole grabbing at the interaction boundary. The
local selected car and a remote owner's accepted car kind use the same rule.
Mario's normal slow/fast grabs and pole positioning remain unchanged. No world
collision, vehicle physics, progression, ability state or proprietary asset
changed. Tree visuals have no solid native triangle mesh to remove.

The test runs actual native hitbox/pole functions and the real adapter against
explicit engine/runtime services. Its 3,128 assertions cover entering/leaving,
repeated jump/boost inputs, zero-input intervals and input capture, host/client
roles, warp/selection/reset and native Mario slow/fast grabs. A pre-fix replay
confirmed suspension and trunk pinning in five assertions. Optional baseline
comparison accepts a caller-supplied revision; the normal test needs no Git
history. This is component integration evidence, not live screenshot acceptance.

The active game was not touched. This fix is local only; the combined Windows
candidate remains to be built.


Combined candidate update: the Windows engine including this change and the configurable speed rule builds successfully. Publication and live gameplay acceptance remain pending; no active game was interrupted. See [car speed verification](LOCAL-CAR-SPEED.md).
