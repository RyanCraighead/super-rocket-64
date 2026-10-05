# Local car-follow camera candidate

Options → Camera → Octane camera selects Mario or Car follow. Car follow is
the default for new or missing settings; an explicit Mario preference persists.
Mario and other characters keep the existing camera. This candidate is local
and has not been published.

Car follow tracks Octane's physical position and heading with a level horizon,
modest speed-dependent distance, and native wall clearance plus a central
occlusion ray. It keeps looking behind the nose while reversing and avoids
rolling the view during flips. The existing right-stick, C-button, mouse,
sensitivity and inversion controls remain available. Look input holds the
chosen angle briefly before following resumes; the native L camera button
recenters. Throttle does not change camera zoom.

The camera releases ownership for native cutscenes and unsupported car states,
holds still while paused, and resets tracking after selection, area, epoch or
large position changes. It never writes the vehicle's position or input state.

Verification: 2,525 assertions exercise the actual camera implementation,
native stick/collision functions and SDL virtual-controller input. They cover
eight headings, both axes and inversion choices, reverse/flip behavior,
recentering, speed distance, wall shortening/recovery, pause, warps, native
handoff and local ownership. Raycast and camera-apply boundaries are explicit
fixture services. The existing 67 SDL direction checks, 80 boost/protocol
checks, and 105 real configuration persistence checks pass. Live visual feel
and the combined Windows build remain pending; the active game was untouched.


Combined candidate update: the Windows engine including this change and the configurable speed rule builds successfully. Publication and live gameplay acceptance remain pending; no active game was interrupted. See [car speed verification](LOCAL-CAR-SPEED.md).
