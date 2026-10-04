# Public release status

**No playable public Windows release has been published.**

The public repository contains documentation and creator-authorized gameplay previews. [Draft PR #1](https://github.com/RyanCraighead/super-rocket-64/pull/1) adds standalone launcher source with headless and isolated-desktop UI test harnesses. A separate local candidate builds from the integrated gameplay baseline, with Octane as the default, the character wheel retained, and the assistant integration removed.

## Publication gate

The original decomp source has a documented CC0 grant. Scoped grants for later PC/multiplayer fork contributions and custom resources remain incompletely documented; that is distinct from the explicit restrictions identified in two excluded SGI files. Some game-derived tables remain in source, and the first local Windows engine included built-in/custom graphics and audio. Omitting standalone ROMs and image files from a ZIP does not establish that the executable is free of embedded game content.

Those source files and engine binaries are not included in this public repository. Before a playable release, the embedded-content inventory must be resolved by local input extraction, replacement with cleared content, or documented redistribution permission. Component-specific licenses must be retained and checked against the actual package. The [content and source audit](CONTENT-AUDIT.md) identifies the concrete files and unresolved categories.

The following remain release gates:

- A public source and binary inventory with documented provenance and no proprietary asset payloads.
- The Windows EXE built from that audited source, with matching hashes and notices.
- Fresh-machine installation and end-to-end cancel/back/retry checks through the launcher UI. Bundled helper validation of the optional profiles is complete on the development machine.
- Launcher UI and gameplay verification, including the retained character wheel and save/controller persistence.
- Matching-build Mario/Octane multiplayer checks. Optional characters are not supported online.

Older gameplay footage and inherited tests are useful development evidence, not certification of a newly packaged release. Native multiplayer, WAN and physical-controller coverage remain limited.

## Development verification

The Windows candidate cross-build passed. A local-only EXE passed clean installation into an isolated folder, then its bundled Python/Unicorn completed SM64/Octane setup using owned local inputs and the verified extraction tool. Retrying with no source inputs reused the validated profile and preserved a synthetic save. Offline, Host and Join arguments were checked without starting the game. This is development evidence, not a public download or a fresh-machine certification.

All five optional profiles now pass the bundled helper's validation: Link, Bomberman, Banjo-Kazooie, Spider-Man and Tony Hawk. Four initial setup commands have captured successful exits; Spider-Man's generated profile was confirmed by reuse and final validation. Bomberman was tested with its supported raw ROM input. Missing/wrong-input failures and a corrected-input retry were also checked. These tests used local owned sources in isolated folders and did not start the game.

A later local migration cross-build externalizes 115 inventoried graphics/audio buffers totaling 40,239,904 bytes. The actual Windows binary passes an early headless validation probe, rejects missing and tampered data, and succeeds after repair. None of the 115 original full byte sequences remains in that binary. The later setup integration described below assembles a complete private profile, but its remaining seed resources are not yet a cleared public recipe. The engine and that folder remain unpublished.

Further verified work reconstructs four Mario wing vertex arrays directly from the supported SM64 ROM, with exact output from the actual C loader. Seven matrix/vector implementation units now use project-authored math; 50,000 matrix and 10,000 vector comparisons pass. The required alSeqFileNew implementation remains in alBnkfNew.c, an exact match to the documented CC0 source. The resulting Windows cross-build and early data probe pass. The detailed content audit distinguishes these resolved items from missing fork/resource grants and unfinished extraction work.

Audio follow-up independently reproduces the 160-byte bank-set metadata exactly and verifies all 34 music sequence ROM overlays. The repository/component-grant scope for the 123 credited custom voice inputs remains unresolved; individual notices are not required if an applicable broader grant covers them; the disabled extra Peach references do not require more user files.

The actual C ROM loader now reconstructs 42 object-placement tables, 28 movement-path tables and 851 base-game light structs: 921 structures / 39,764 bytes. Their output exactly matches the previous compiled definitions. Sanitizer tests cover zero initialization, native-endian signed words, complete light layouts and 18 invalid-span cases with unchanged outputs. The Windows build and early data probe pass; all 921 selected symbols occupy uninitialized storage in the linked EXE. Sixty unmatched light definitions remain unchanged, and custom player actors were excluded from the light batch. This work uses the existing supported SM64 ROM and changes no presentation values.

The launcher passes 29 headless checks. Helper tests cover input failures, cancellation, stable data paths and cache validation; an actual corrupted-profile repair preserved save/config bytes and kept a recovery copy. Focused wheel, networking, Whomp, blue-switch, coin-boost and boost-setting persistence suites also passed. Fresh-machine setup, native gameplay, physical-controller and two-PC acceptance remain pending. These results do not resolve the publication gate above.

## Name check

An exact GitHub repository-name search for `super-rocket-64` returned no results before this repository was created. This was an obvious-conflict check, not trademark clearance.

The actual launcher UI now also passes 48 checks on a separate, never-activated Windows desktop, with normal/minimum-size page renders inspected. A private real-payload run passed 64 UI checks covering missing-input failure, cancel before helper startup, clean Octane extraction, retry, and source-free reuse with a synthetic save preserved. This found and fixed a launcher precheck that had unnecessarily demanded original inputs before allowing the helper to validate reusable assets. The child never became foreground; no gameplay, physical controller or peer connection was tested.

The initial root-file/README review was incomplete as a lineage and dependency review. The newer upstream evidence below governs the assessment; inherited grants and scoped permissions are retained. No broad feature-removal or speculative replacement plan is being applied.

The next private Windows build also reconstructs three pointer-free display lists (13 commands / 104 ROM bytes), translating original F3D commands into the current PC renderer format. Exact old/PC-format comparisons and 24 rejected-input checks pass under sanitizers. A generated DynOS declaration conflict was fixed while preserving all 2,771 registry names. The repaired Windows build and 115-buffer early probe pass; all 924 selected structures occupy uninitialized storage in the EXE. The previous verified executable is preserved. No game or UI was started for these checks.

The current private Windows payload integrates the owned-ROM audio recipe into actual setup and launch. It creates and verifies the shared 115-file data profile, extracts 34 music ranges and 297 sample ranges (3,341,941 bytes), and generates the 160-byte sequence-bank metadata from 35 index lists. An actual C-loader comparison under sanitizers proves the assembled complete audio buffers equal the previous runtime output byte for byte. Later runtime overlays are idempotent; custom bytes and presentation are preserved.

This payload passed 49 clean-setup/reuse/repair/cancellation/launch-wiring checks and 64 real launcher UI checks on a separate, never-activated desktop. Missing or changed shared files prevent a false ready state; setup repairs them from the saved normalized ROM without requesting the original Rocket League input again. Valid tool/data caches are not rewritten. Save/controller sentinels and character profiles remain unchanged. The exact packaged engine validates all 115 buffers before any game window, audio or network startup. Offline/Host/Join environment handoffs were captured without starting gameplay.

The integration is functional but still uses 114 private validation seed inputs for the remaining complete graphics/control/custom-audio data. Those seeds and the private EXE are excluded from this repository. A successful private install is not yet a proprietary-payload-free public distribution. The prior verified executables and packages remain preserved.

The source review now follows the actual lineage through SM64coopdx, the pre-deletion `djoslin0/sm64ex-coop` history, `sm64pc/sm64ex`, `sm64-port`, and n64decomp. It keeps inherited CC0-covered source separate from later fork-specific contributions. A missing root license is not a blanket prohibition.

The renderer's [newer upstream license](https://github.com/Emill/n64-fast3d-engine/blob/881eb68bad1150f720433d75bccb4d26879c28ab/LICENSE.txt) permits binary redistribution when the binary contains no assets that the distributor lacks rights to distribute. Its required notice/conditions are retained in the private candidate; the stale inherited README's older binary prohibition is not treated as current. Upstream also has an explicit [Flathub distribution approval](https://github.com/coop-deluxe/sm64coopdx/issues/1112#issuecomment-4413172616), which is positive permission evidence for that proposal. Its scope is considered alongside the [community-port invitation](https://github.com/coop-deluxe/sm64coopdx/discussions/1224), not silently extended to every downstream project or third-party asset.

Our actual Windows build sets `COOPNET=0`. Resolved link flags contain neither `libcoopnet` nor `libjuice`, and the linked map contains none of the 12 external CoopNet API entries or libjuice transport symbols. The inherited fork's ID/compatibility helpers remain distinct. The upstream default alone therefore does not establish a static CoopNet/libjuice obligation for this build. Other enabled dependency terms still apply.
