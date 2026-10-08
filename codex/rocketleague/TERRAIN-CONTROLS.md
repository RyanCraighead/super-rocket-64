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
