# Public release status

**No playable public Windows release has been published.**

The public repository contains documentation and creator-authorized gameplay previews. [Draft PR #1](https://github.com/RyanCraighead/super-rocket-64/pull/1) adds standalone launcher source with headless and isolated-desktop UI test harnesses. A separate local candidate builds from the integrated gameplay baseline, with Octane as the default, the character wheel retained, and the assistant integration removed.

## Publication gate

The original decomp source has a documented CC0 grant. Scoped grants for later PC/multiplayer fork contributions and custom resources remain incompletely documented; that is distinct from the explicit restrictions identified in two excluded SGI files. Some game-derived tables remain in source, and the first local Windows engine included built-in/custom graphics and audio. Omitting standalone ROMs and image files from a ZIP does not establish that the executable is free of embedded game content.

Those source files and engine binaries are not included in this public repository. Before a playable release, the embedded-content inventory must be resolved by local input extraction, replacement with cleared content, or documented redistribution permission. Component-specific licenses must be retained and checked against the actual package. The [content and source audit](CONTENT-AUDIT.md) identifies the concrete files and unresolved categories.

The following remain release gates:

- A public source and binary inventory with documented provenance and no proprietary asset payloads.
- The Windows EXE built from that audited source, with matching hashes and notices.
- Fresh-machine installation; development-machine cancel/back/retry through the actual launcher UI is verified. Bundled helper validation of the optional profiles is complete on the development machine.
- Broader gameplay coverage is documented honestly: automated UI/save preservation and Ryan's successful v5 manual test are recorded; physical-controller and all-character coverage are not inferred.
- Matching-build two-PC Mario/Octane multiplayer coverage remains unverified. Optional characters are not supported online.

Older gameplay footage and inherited tests are useful development evidence, not certification of a newly packaged release. Native multiplayer, WAN and physical-controller coverage remain limited.

## Development verification

The Windows candidate cross-build passed. A local-only EXE passed clean installation into an isolated folder, then its bundled Python/Unicorn completed SM64/Octane setup using owned local inputs and the verified extraction tool. Retrying with no source inputs reused the validated profile and preserved a synthetic save. Offline, Host and Join arguments were checked without starting the game. This is development evidence, not a public download or a fresh-machine certification.

All five optional profiles now pass the bundled helper's validation: Link, Bomberman, Banjo-Kazooie, Spider-Man and Tony Hawk. Four initial setup commands have captured successful exits; Spider-Man's generated profile was confirmed by reuse and final validation. Bomberman was tested with its supported raw ROM input. Missing/wrong-input failures and a corrected-input retry were also checked. These tests used local owned sources in isolated folders and did not start the game.

A later local migration cross-build externalizes 115 inventoried graphics/audio buffers totaling 40,239,904 bytes. The actual Windows binary passes an early headless validation probe, rejects missing and tampered data, and succeeds after repair. None of the 115 original full byte sequences remains in that binary. The later source-recipe integration described below assembles a complete private profile without full-buffer seed dependencies; publication scope remains under review. The engine and that folder remain unpublished.

Further verified work reconstructs four Mario wing vertex arrays directly from the supported SM64 ROM, with exact output from the actual C loader. Seven matrix/vector implementation units now use project-authored math; 50,000 matrix and 10,000 vector comparisons pass. The required alSeqFileNew implementation remains in alBnkfNew.c, an exact match to the documented CC0 source. The resulting Windows cross-build and early data probe pass. The detailed content audit distinguishes these resolved items from missing fork/resource grants and unfinished extraction work.

Audio follow-up independently reproduces the 160-byte bank-set metadata exactly and verifies all 34 music sequence ROM overlays. The repository/component-grant scope for the 123 credited custom voice inputs remains unresolved; individual notices are not required if an applicable broader grant covers them; the disabled extra Peach references do not require more user files.

The actual C ROM loader now reconstructs 42 object-placement tables, 28 movement-path tables and 851 base-game light structs: 921 structures / 39,764 bytes. Their output exactly matches the previous compiled definitions. Sanitizer tests cover zero initialization, native-endian signed words, complete light layouts and 18 invalid-span cases with unchanged outputs. The Windows build and early data probe pass; all 921 selected symbols occupy uninitialized storage in the linked EXE. Sixty unmatched light definitions remain unchanged, and custom player actors were excluded from the light batch. This work uses the existing supported SM64 ROM and changes no presentation values.

The launcher passes 29 headless checks. Helper tests cover input failures, cancellation, stable data paths and cache validation; an actual corrupted-profile repair preserved save/config bytes and kept a recovery copy. Focused wheel, networking, Whomp, blue-switch, coin-boost and boost-setting persistence suites also passed. Fresh-machine, physical-controller and two-PC coverage remain unspecified. Ryan subsequently reported successful manual testing of v5; see the current baseline update below. These results do not resolve the publication gate above.

## Name check

An exact GitHub repository-name search for `super-rocket-64` returned no results before this repository was created. This was an obvious-conflict check, not trademark clearance.

The actual launcher UI now also passes 48 checks on a separate, never-activated Windows desktop, with normal/minimum-size page renders inspected. A private real-payload run passed 64 UI checks covering missing-input failure, cancel before helper startup, clean Octane extraction, retry, and source-free reuse with a synthetic save preserved. This found and fixed a launcher precheck that had unnecessarily demanded original inputs before allowing the helper to validate reusable assets. The child never became foreground; no gameplay, physical controller or peer connection was tested.

The initial root-file/README review was incomplete as a lineage and dependency review. The newer upstream evidence below governs the assessment; inherited grants and scoped permissions are retained. No broad feature-removal or speculative replacement plan is being applied.

The next private Windows build also reconstructs three pointer-free display lists (13 commands / 104 ROM bytes), translating original F3D commands into the current PC renderer format. Exact old/PC-format comparisons and 24 rejected-input checks pass under sanitizers. A generated DynOS declaration conflict was fixed while preserving all 2,771 registry names. The repaired Windows build and 115-buffer early probe pass; all 924 selected structures occupy uninitialized storage in the EXE. The previous verified executable is preserved. No game or UI was started for these checks.

The current private Windows installer reproduces all 115 externalized shared runtime files from original source components, generated metadata and the existing owned-ROM input. It contains **zero private full-buffer seed files**. This resolves the technical source routes for all 113 former seed dependencies: 107 PNG conversions, four inline source textures, and two mixed source/ROM audio buffers. No additional user-selected game, recording, Python installation or extraction executable is required.

The standard-library visual converter reproduces all 111 visual outputs exactly. The music path uses a reproducible source-built sound-player prefix, zero-filled alignment gaps and 34 owned-ROM spans. A narrowly asserted build-stage source adjustment keeps the validated Toad note velocity at 127 instead of the source file's 100; the original source stays preserved. The sample path uses 123 recordings encoded during the project build, the existing bank metadata, and 297 owned-ROM spans. The complete music, sample and sound-control buffers all match their registered hashes. Compilers and audio encoders are maintainer build tools; they are not installed or executed on the user's Windows machine.

The exact v5 payload passed 71 setup/reuse/repair/cancellation/source-integrity/launch-wiring checks and 64 actual launcher UI checks on a separate, never-activated desktop. Six additional upgrade checks passed against a copy of the v4 data profile: no original input arguments were needed; save/controller/character bytes and timestamps, tool caches, the old shared cache and the original v4 profile remained unchanged. The engine EXE is unchanged, and its early probe validates all 115 generated buffers. No native gameplay or peer connection was started.

Source reproducibility is distinct from permission to publish original/custom resources or other embedded engine material. The scoped source/resource review remains unresolved. Ryan subsequently reported successful manual testing; broader controller and two-PC coverage is not inferred. The public preview still excludes the engine, source-resource pack and playable EXE. All earlier verified installers, source inputs, character features and presentation are preserved.

The source review now follows the actual lineage through SM64coopdx, the pre-deletion `djoslin0/sm64ex-coop` history, `sm64pc/sm64ex`, `sm64-port`, and n64decomp. It keeps inherited CC0-covered source separate from later fork-specific contributions. A missing root license is not a blanket prohibition.

The renderer's [newer upstream license](https://github.com/Emill/n64-fast3d-engine/blob/881eb68bad1150f720433d75bccb4d26879c28ab/LICENSE.txt) permits binary redistribution when the binary contains no assets that the distributor lacks rights to distribute. Its required notice/conditions are retained in the private candidate; the stale inherited README's older binary prohibition is not treated as current. Upstream also has an explicit [Flathub distribution approval](https://github.com/coop-deluxe/sm64coopdx/issues/1112#issuecomment-4413172616), which is positive permission evidence for that proposal. Its scope is considered alongside the [community-port invitation](https://github.com/coop-deluxe/sm64coopdx/discussions/1224), not silently extended to every downstream project or third-party asset.

Our actual Windows build sets `COOPNET=0`. Resolved link flags contain neither `libcoopnet` nor `libjuice`, and the linked map contains none of the 12 external CoopNet API entries or libjuice transport symbols. The inherited fork's ID/compatibility helpers remain distinct. The upstream default alone therefore does not establish a static CoopNet/libjuice obligation for this build. Other enabled dependency terms still apply.

## Goddard reconstruction and current baseline

The earlier embedded face/eye finding is fixed. The actual C loader now reconstructs all 12 literal geometry arrays and all nine DynList command tables in the 13 Goddard dynlist source files from the same supported owned SM64 ROM. This covers 13,568 geometry bytes and 1,303 commands / 31,272 ROM bytes. All 63 host-pointer bindings are explicit. Complete native structures match the original definitions, including float bits, padding and pointers; sanitizer tests cover zero initialization and 21 rejected malformed command/binding/span cases with unchanged output. The 25 remaining animation descriptors contain counts, types and pointers to already ROM-loaded animation data.

The final Windows build places all 21 selected arrays in uninitialized storage, with zero file bytes for those symbols. The 12 original full geometry sequences are absent from the engine. No literal DynList initializers remain in those 13 source files. This is a specific verified result, not clearance of every other actor/level command table.

The separate v7 installer passed 71 setup, repair, cancellation, integrity and launch-wiring checks; 64 actual UI checks on a never-activated Windows desktop; and six copied-profile upgrade checks. The setup produces all 115 shared files at their expected hashes. Existing structural-table, light, scalar-display-list and wing-loader sanitizer regressions also pass. The package audit verifies all 569 manifest files and 1,862 entries including nested dependency archives. The v5 and v6 installers remain byte-identical to their preserved versions.

The package retains component notices for the actual linked dependencies, including the updated conditional renderer grant, original-decomp CC0 notice, SDL2, GLEW, zlib, RocketSim/Bullet, MinGW/GCC runtime terms and the exact MIT-style miniz notice. Existing CPython, Unicorn corresponding source, font and extraction-tool notices remain included. The actual final engine is built with `DISCORD_SDK=0 COOPNET=0 UPDATER=0`; its imports/map contain no Discord SDK, external CoopNet API or libjuice linkage. Fork-owned compatibility helpers are not SDK linkage.

Ryan reported that the v5 installer was working after manual testing. That success is accepted; no repeat test is requested. It does not specify all-character, physical-controller or two-PC coverage. The automated v7 checks did not start gameplay or network peers and did not take the foreground.

The remaining publication decision is the applicable grant for used later fork contributions (including `src/pc/network/network_player.c` and later Lua/DJUI/DynOS changes) and retained original custom resources: 107 PNG sources, four apparition textures and 123 credited custom voice recordings with their derived encodings. The original CC0 grant, component licenses, explicit Flathub approval and community-port invitation are positive scoped evidence; the current record does not establish their full coverage of this derivative and those resources. This is not a finding that a missing per-file notice forbids distribution. No feature/resource removal or external outreach is proposed. Other linked actor/level command tables also remain outside the completed bounded proof. The engine, source-resource pack and playable EXE remain private pending that exact scope decision and final export audit.

Updater and optional-shortcut work is paused for the second release. Neither is included in this baseline.
