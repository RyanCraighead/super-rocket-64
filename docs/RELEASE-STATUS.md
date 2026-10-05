# Verification and limitations

## v0.2.10: independent Octane jump height

Options > Octane jump height (%) defaults to 50%, with 50–100% available and
100% restoring original jump physics. The calibrated initial/held lift gives
roughly half the single-jump height while preserving gravity and the hold
window. The host chooses the online rule; saved client preferences remain
local. Everyone in an online session must update. Speed stays independent.

All 13 physics tests pass, including 250,020 jump-height assertions. Original
100% physics matches the previous backend byte for byte across 1,920 frames.
Production rule/config tests and native gameplay regressions pass. Owned
King/button geometry replays pass at jump 50% with speed 50% and 75%.
The Windows engine builds. See [jump height measurements and limits](CAR-JUMP-HEIGHT.md).
Reduced unboosted ledge reach may need a held/double jump, boost or 100%.
Full playthrough, physical-controller feel and two-PC/WAN testing remain pending.


## v0.2.9 camera, contacts and configurable speed

This release adds non-damaging car/player bumps, a saved car-follow camera,
prevents Octane from entering Mario's native tree-climbing state, repairs the
Whomp King surface eligibility and real-size blue-switch impact witness, and
adds a host-controlled 50-100% speed setting (75% default, 100% original).
The combined Windows engine and standalone EXE build. The package audit
checks 1,867 entries and 777 notice paths with no flagged files. The actual
EXE preserves all 132 copied data files and timestamps, plus launcher
preferences, and verifies the 115 shared engine-data files. All 12 backend tests pass, including
9,609 speed-physics and 191 impact-boundary assertions. The 100% replay matches
the saved previous backend byte for byte across 1,920 frames/eight scenarios.
Owned native collision/behavior replay passes 16,423 assertions at each of
50% and 75%, including three King hits/one star and blue-switch flips.
Rule/config/network, camera, tree, platform, cap and traversal regressions
pass. Details and scope are in [the speed audit](LOCAL-CAR-SPEED.md) and the
linked local-change documents. Full-game feel, physical controllers and
two-PC/WAN acceptance remain unverified. No live game was used for these tests.


This first Windows release keeps Octane as the default, the character wheel,
existing gameplay/progression fixes and direct-IP Mario/Octane multiplayer.
The assistant integration is removed. v0.2.0 adds opt-in update checks,
confirmed installation/rollback and optional persistent launcher shortcuts.

The preserved baseline passed 71 setup/repair/cancel/integrity/launch checks,
64 actual launcher UI checks on a separate never-activated desktop and six
upgrade checks against a copied profile. The user also reported successful
manual play of the earlier v5 baseline. Those are development-machine results;
they do not establish all-character, physical-controller or two-PC/WAN coverage.
Optional characters are offline only.

The v0.1.0 release was rebuilt from the public source export. Its standalone
EXE passed windowless installation and complete payload hash verification.
Fresh setup from the owned SM64 and supported Rocket League inputs passed,
including all 115 shared runtime files and the engine's headless validation.
Reusing the setup without source paths preserved profile bytes, timestamps,
the test save, shared assets and the verified extraction-tool cache. The
complete payload and nested runtime/source archives passed the private-data
and notice scan. These final checks did not enter gameplay or use the desktop.

The public edition is exported from an explicit source allowlist without
private Git history. The ROM, Rocket League packages, extracted character
profiles and saves remain local inputs. Setup reconstructs shared game data
from the supported owned ROM and declared original/source-built components.

## v0.1.1 setup update

Epic Games Store installation discovery reads completed local Epic manifests.
Setup accepts Epic or Steam installations whose two required package hashes
match the supported profile, and rejects missing/unsupported packages before
provisioning extraction tools. No Steam client is required. The game engine,
converted geometry checks and multiplayer protocol are unchanged.

The new launcher passed 36 headless checks, 48 UI checks on a never-activated
desktop and five extraction/preflight checks. The rebuilt EXE passed payload
verification, clean owned-input setup, all 115 shared-file checks, headless
engine validation and source-free reuse preserving profile/save/cache bytes
and timestamps. The payload and nested archives passed the private-data scan.
Automated extraction was tested with the verified Steam package pair.
On October 5, 2026 at 01:31 UTC, Ryan reported: "I tested the epic installation
and it worked." Epic installation is now user-verified. This report adds
manual user evidence; it does not extend the automated test coverage or
establish compatibility with every future game update. Two-PC/WAN and new
physical-controller acceptance remain pending.

Public-source research also confirms the shared package layout in the
[VelocityRL author's source](https://github.com/bitsfdb/VelocityRL/blob/61e96ff0a6e0047f75fe73d72effc793c48aecce/src-tauri/src/integrity.rs).
This corroborates folder/container structure, not byte-for-byte identity of
uninspected Epic packages. No third-party implementation or game data was copied.

See [content and attribution](CONTENT-AUDIT.md), [supported inputs](ASSET-SOURCES.md)
and [building from source](../BUILDING.md). Matching release hashes are provided
with each release; do not assume an older development installer is identical.

The v0.2.0 launcher passed 36 baseline checks, 22 updater/shortcut checks and
85 actual UI checks on a never-activated desktop. Checks cover offline/no-release
responses, manifest/version mismatches, partial downloads, cancellation, actual
child probes/restart handshakes, failed restart rollback, operation locking and
actual shortcut target/argument verification. The game engine is unchanged.

The final v0.2.0 EXE passed a live official GitHub HTTPS check, its embedded
update probe and an upgrade/reuse test against copied v0.1.1 data. All 130 data
files retained their bytes and timestamps, including assets, saves and tool
cache. Its actual restart UI was exercised on a never-activated desktop:
game controls stayed disabled until the matching commit signal, then enabled;
the launcher closed normally. The 1,864-entry payload/nested archive audit had
no private-data or prohibited-input findings. These checks did not launch a game
or alter real Desktop/Start Menu shortcuts, networking or security settings.

## v0.2.1 folder picker simplification

Rocket League setup now uses one Browse folder picker for Epic Games Store and
Steam. The separate Epic discovery button and unused manifest code are removed;
package checks and extraction are unchanged. Existing updater, shortcuts,
assets, saves and configuration remain supported.

The patch passed 31 headless checks, 22 updater/shortcut checks and 95 UI checks
on a never-activated desktop, including both store folder layouts and Back
navigation. The actual packaged EXE passed integrity/probe/restart checks and
live GitHub HTTPS. Reusing copied v0.2.0 data preserved all 130 data files and
launcher preferences. The 1,864-entry payload audit had no findings. Epic remains
user-verified; two-PC/WAN and new physical-controller acceptance remain pending.

## v0.2.2 ship entrance patch

The inherited CCM chimney bridge is present in all public releases. JRB uses a
different floor-warp entrance: an upright Octane chassis can catch on the tilted
porthole before its center reaches the trigger. The new bridge recognizes the
loaded, authored opening after the eel leaves and calls the native floor-warp
operation. It does not change collision, save flags, eel behavior, chest puzzles,
star requirements, or the multiplayer protocol.

The focused adapter suite passed 341 JRB and 546 CCM assertions with ASan/UBSan,
including a chassis sweep into the tilted wall and offline/host/client guards.
The new ship test fails against the published v0.2.1 adapter. The public host,
geometry, gamepad and export checks passed, along with 31 headless launcher and
22 updater checks. Stale assistant-panel mocks and a private-only CLI test
reference were corrected in the public suite.

These checks use explicit runtime/native mocks; they are not live game or
two-PC acceptance. No foreground or gameplay session was started. The Windows
release also undergoes package integrity, privacy/notice and copied-profile
reuse checks before publication.

## v0.2.3 stained-glass entrance patch

The reported castle window is the Princess's Secret Slide entrance: castle
area 1, object warp 0x0A to LEVEL_PSS. Its diagonal mouth is about 154 units
wide, narrower than Octane's 173-unit chassis. The focused bridge stages the
existing native warp when the car touches the actual alcove opening with an
unobstructed path inside. It leaves the one-star room door, collision, slide
timer, chests, stars, save flags and multiplayer protocol unchanged.

Passed 436 PSS assertions plus the existing 341 JRB and 546 CCM assertions
under ASan/UBSan, and the headless host/geometry/gamepad/export suite. The new
slide case fails against released v0.2.2 and passes after the fix. An additional
private replay used all 2,144 castle collision triangles from the owned ROM
and confirmed five approach heights. That geometry was not added to public
source or the release. These checks exercise the real adapter with explicit
runtime/native mocks; live gameplay and two-PC acceptance remain pending.

Version 0.2.2 was published and its public download verified before this
follow-up began. The two fixes are separate releases; v0.2.3 includes v0.2.2.

## v0.2.4 door orientation and controller handoff

Octane now presents the native door action's travel heading on either side,
including the final frame and destination spawn. This corrects a 180-degree
display flip caused by copying Mario's authored animation root yaw. Native
actions, warp timing, save state and the multiplayer protocol are unchanged;
the corrected presentation snapshot uses the existing network packet path.

Selected car bindings stay reserved while native animations temporarily own
movement. Holding R2/RT therefore cannot become Mario's camera button during
the handoff. A held binding must be released before taking a new meaning after
a character switch or rebind. Deliberate unassigned camera controls and fresh
menu/dialog confirmation still work.

Headless validation passed 624 checks executing the actual push/pull action and
door-spawn code with explicit animation-service mocks, 208 presentation cases
with actual packet-codec round trips, and 133 handoff checks using the real SDL
reader with a process-local virtual controller. Both new regressions fail
behaviorally against v0.2.3. Existing network/transport tests and all 1,323
PSS/JRB/CCM entrance assertions also passed with ASan/UBSan.

These tests do not establish live animation playback, physical-controller or
two-PC acceptance. No game or foreground window was started. The release is
checked for package integrity, private/prohibited inputs, notices, and reuse
of copied assets, saves and preferences before publication.

## v0.2.5 contextual car interaction

The v0.2.4 controller route was checked before implementation: Octane's Cross/A
jump was deliberately isolated from native Mario A/B, so signs and NPCs never
received the press. There was no dedicated car interact binding. Keyboard L
still reached native A; that did not make controller interaction work.

A fresh configured car Jump press now reaches only the existing native text
handler for a collided nearby sign/NPC, while grounded and upright. Native
facing/front-side, action and dialog checks remain authoritative. A rejected
target leaves ordinary car jumping intact. The bridge checks only the local
player, rejects stale poses and unsynchronized areas, and changes no packets,
NPC progress, object reach or save state. Holding a press through handoffs
requires release before another interaction. The remapped Jump button can also
advance native text after the existing controller UI release gate.

Validation passed 150 native-text checks and 51 real-SDL dialog/mapping checks,
including held/released/repeated input, Cross/R1/R2 remaps, native facing and
collision membership, ordinary jump fallback, dialog return, and local
offline/host/client gates. Both new behavior tests reject v0.2.4. The existing
133 door-controller checks, 624 native door checks, 208 presentation cases,
network/transport suite and 1,323 entrance assertions remain passing under
ASan/UBSan. Native push-out/animation services are explicit test mocks.

Other characters retain their existing mappings. In particular, Tony's SDL
Cross maps to ollie/C-down; this patch does not claim universal controller
interaction support for all optional characters. Mario retains native Cross
or Circle text interaction. Live gameplay, physical controllers and two-PC
acceptance remain pending; no foreground or game session was started.


## v0.2.6 car handling, native interactions and optional sounds

The Octane free-camera horizontal direction now matches the installed Rocket
League factory mapping; vertical was already correct. Explicit inversion
preferences remain available. New/missing surface settings choose Native;
saved Car grip and Native choices are preserved. Only the static native ice
inside CCM's slide receives the restricted 0.25 grip needed for steering and
braking. Other ice, moving surfaces and Car grip retain their previous rules.

The CCM chimney bridge accepts valid edge landings inside the actual opening,
including after native warp/reset cycles. An upright car resting all four tires
on an exposed prone Whomp back can now damage it, with native vulnerability,
health/cooldown and host authority. Flip/dive attacks and blue switches remain.

Setup can extract verified jump, flip, double-jump and standard boost effects
from the player's supported Rocket League banks using pinned official tools.
Options > Octane sounds saves Mario or Car locally. Missing optional audio
falls back to Mario. Only the local simulated car emits these new events;
remote render poses do not produce duplicate sound. Boost follows actual
thrust/fuel and has separate start, loop and end clips. Pause, blocked input,
warp/selection handoff and shutdown clear the voices. Master/SFX mute and
volume use the existing game output; no second audio device is opened.

Focused headless checks passed: 67 actual camera direction cases, 80 session
rule cases, 82 config persistence cases, 56 native surface classifications and
6,225 actual RocketSim environment assertions. CCM passed 780 adapter and
2,801 native warp assertions; Whomp passed real-physics landing and native
local/remote authority, vulnerability and contact regressions. The optional
sound suite passed 395 actual mixer/event/physics checks, 18 native local
sound-source checks and eight synthetic extraction/cache/cancel tests. Six
owned clips decoded to the pinned hashes, loaded in the runtime mixer, and
reused with unchanged bytes and timestamps. These assets remain private.

This is headless regression coverage with explicit native-service fixtures,
not live gameplay, physical-controller, audio-listening or two-PC acceptance.
No active game was interrupted or brought to the foreground. Epic geometry
support retains the earlier user verification; the new audio profile was
automatically verified from Steam files and accepts only matching Epic banks.


## v0.2.7 Whomp crush handling

Octane hands Whomp underside impacts to native squish, damage and recovery
before the rigid chassis can be trapped through the floor. The probe covers
the full car footprint, including edge and inverted roof contacts, and requires
nearby native support. Native Metal/invincibility, injury, death and recovery
remain authoritative. No Whomp collision faces are removed. Riding on its back
as it stands still uses the existing kinematic launch; upright vulnerable-back
attacks retain their prior authority and cooldown rules.

The car visibly flattens and recovers. Local drawing follows native scale;
peers use the existing action/squish timer fields without a packet-version
change or remote damage simulation. A remote crush starts at the compressed
pose because its ceiling geometry may be a different simulation frame.

The generated real-physics reproduction penetrated the floor at three lateral
positions before the handoff; the guarded runs stayed above it. The same
backend still launches a car riding the rising back. Focused native-action
fixtures cover edge/roof contacts, repeated frames, offline/local host/client
ownership, Metal/invincibility, moving support, death, recovery, state reset
and local/remote scale. Existing Whomp attacks, dynamic-platform lifecycle,
entrance bridges, presentation and packet ingress are regression-tested.

These are headless component tests with explicit service fixtures. Live
Whomp gameplay, visual acceptance and two-PC testing remain pending. The
active game, controller, saves and foreground were not used for these tests.


## v0.2.8 native pipe entry

Octane could balance on a standard pipe rim above its native warp cylinder.
The reproduced fault affects the Bowser 1 and 3 lead-up pipes and all six
Tiny-Huge Island size-change pipe endpoints. The adapter now stages a native
warp interaction only for contact with the actual loaded, upright pipe rim,
inside native horizontal reach, while descending or resting. Native dispatch
still controls the destination, transition, emergence and re-entry cooldown.
Blocked, stale, remote, scaled, altered and unsynchronized contacts are rejected.

All nine native pipe/hole routes were audited. Bowser 2's larger floor hole
already receives the car and is unchanged. Castle doors, star/key requirements,
endless stairs, the CCM chimney, other warps and all save data are unchanged.

Private owned geometry and actual RocketSim poses reproduced the original
standard-pipe hang at four headings; the unmodified adapter failed the new
behavior test. The corrected adapter passed 18,618 assertions with those
inputs and 849 generated-fixture assertions. Exact native collision/dispatch
bodies and route metadata passed 26,568 checks across all nine routes,
including local offline/host/client ownership, repeat visits, emergence and
the native 30-frame re-entry timer. These are headless component tests with
explicit service boundaries, not live gameplay or two-PC acceptance. No
extracted geometry or game assets are included in the source or release.
