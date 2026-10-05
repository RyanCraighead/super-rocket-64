# Local Whomp and blue-switch integration fixes

The native object allocator initializes `oIntangibleTimer` to -1. Whomps are
surface objects: their solid back/underside triangles remain active with that
value. The crossover attack and crush gates incorrectly required zero. Both
now use the native vulnerable action, lifecycle and actual solid triangles.
Death, shake/rise, hidden objects and genuinely intangible surfaces remain
ineligible. Four upright contacting tires can damage a prone Whomp King
without a flip or boost, once per native vulnerability cycle.

The blue-switch detector previously tested the globally lowest chassis point
and required the car origin inside the button. A small button can contact a
different part of the rotating chassis while the nose is beyond it. Its
support witness now comes from clipping the six chassis faces to the button's
native footprint. The existing flip/dive intent, downward entry, timing,
continuity, edge clearance and real native floor/path checks still apply.
Passive parking never activates blue switches. No collision is removed.

Verification includes the production Whomp/switch consumers, real transforms,
triangle construction, floor queries and path checks against privately
extracted owned US ROM collision streams. Real pinned RocketSim snapshots
produced 16,422 passing integration assertions: small Whomp loot, three King
hits and one star, eight headings, local/remote source poses under offline,
host and client contexts, and blue-button flip activation/rejection. Audio,
rendering, network delivery, surface allocation and native world movement are
explicit fixture boundaries. This is headless component integration, not a
running-game or two-PC acceptance claim.

The same 714 real-size blue-button input trajectories accepted 30 contacts
before and 99 after the footprint correction; this is diagnostic coverage,
not a success-rate target. The existing native Whomp/switch regression and
2,950 native crush assertions also pass with the correct allocation default.

Run the asset-free regression with `bash codex/rocketleague/tests/test_whomp.sh`
and `test_whomp_crush.sh`. The optional `test_attack_native_geometry.sh` takes
four local fixture paths: Whomp and switch native collision streams as
little-endian signed 16-bit words, followed by a sequence of host-ABI
RocketSnapshot records for the switch and one settled King-back snapshot.
These inputs are generated from owned assets and the current pinned physics
build, never distributed in Git or releases.

All changes remain local. The active game, controller, saves and foreground
were untouched. The combined Windows candidate is still pending.
