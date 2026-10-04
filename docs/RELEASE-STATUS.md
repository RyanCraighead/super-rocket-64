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

A later local migration cross-build externalizes 115 inventoried graphics/audio buffers totaling 40,239,904 bytes. The actual Windows binary passes an early headless validation probe, rejects missing and tampered data, and succeeds after repair. None of the 115 original full byte sequences remains in that binary. Its private data folder still has unresolved source/rights requirements, and the launcher has not been integrated with a cleared, reproducible way to create it. The engine and that folder remain unpublished.

Further verified work reconstructs four Mario wing vertex arrays directly from the supported SM64 ROM, with exact output from the actual C loader. Seven matrix/vector implementation units now use project-authored math; 50,000 matrix and 10,000 vector comparisons pass. The required alSeqFileNew implementation remains in alBnkfNew.c, an exact match to the documented CC0 source. The resulting Windows cross-build and early data probe pass. The detailed content audit distinguishes these resolved items from missing fork/resource grants and unfinished extraction work.

Audio follow-up independently reproduces the 160-byte bank-set metadata exactly and verifies all 34 music sequence ROM overlays. The repository/component-grant scope for the 123 credited custom voice inputs remains unresolved; individual notices are not required if an applicable broader grant covers them; the disabled extra Peach references do not require more user files.

The actual C ROM loader now reconstructs 42 object-placement tables, 28 movement-path tables and 851 base-game light structs: 921 structures / 39,764 bytes. Their output exactly matches the previous compiled definitions. Sanitizer tests cover zero initialization, native-endian signed words, complete light layouts and 18 invalid-span cases with unchanged outputs. The Windows build and early data probe pass; all 921 selected symbols occupy uninitialized storage in the linked EXE. Sixty unmatched light definitions remain unchanged, and custom player actors were excluded from the light batch. This work uses the existing supported SM64 ROM and changes no presentation values.

The launcher passes 29 headless checks. Helper tests cover input failures, cancellation, stable data paths and cache validation; an actual corrupted-profile repair preserved save/config bytes and kept a recovery copy. Focused wheel, networking, Whomp, blue-switch, coin-boost and boost-setting persistence suites also passed. Fresh-machine setup, native gameplay, physical-controller and two-PC acceptance remain pending. These results do not resolve the publication gate above.

## Name check

An exact GitHub repository-name search for `super-rocket-64` returned no results before this repository was created. This was an obvious-conflict check, not trademark clearance.

The actual launcher UI now also passes 48 checks on a separate, never-activated Windows desktop, with normal/minimum-size page renders inspected. A private real-payload run passed 64 UI checks covering missing-input failure, cancel before helper startup, clean Octane extraction, retry, and source-free reuse with a synthetic save preserved. This found and fixed a launcher precheck that had unnecessarily demanded original inputs before allowing the helper to validate reusable assets. The child never became foreground; no gameplay, physical controller or peer connection was tested.

A bounded follow-up found no root-license history for the checked names and no project-wide distribution grant in the pinned README, contribution-file inventory, reviewed wiki index or official release notes. Component notices and original-decomp CC0 remain positive scoped evidence. Missing public terms are distinct from an express restriction; no broad feature-removal or speculative replacement plan is being applied.

The next private Windows build also reconstructs three pointer-free display lists (13 commands / 104 ROM bytes), translating original F3D commands into the current PC renderer format. Exact old/PC-format comparisons and 24 rejected-input checks pass under sanitizers. A generated DynOS declaration conflict was fixed while preserving all 2,771 registry names. The repaired Windows build and 115-buffer early probe pass; all 924 selected structures occupy uninitialized storage in the EXE. The previous verified executable is preserved. No game or UI was started for these checks.

A separate setup component now extracts 34 music ranges and 297 sample ranges (3,341,941 bytes) from the existing supported owned SM64 ROM. Thirteen extraction/reuse/corruption/repair/cancellation checks pass, as does an independent comparison with the actual C runtime loader under sanitizers. It runs with the existing bundled Python and requires no extra user input or tool installation. This is a partial recipe component: it is not yet wired into the launcher and does not supply the current engine's custom graphics/audio or complete sound metadata/control program. Its success explicitly does not mean a complete engine profile is ready.

The upstream repository's official [General discussion about unofficial ports](https://github.com/coop-deluxe/sm64coopdx/discussions/1224) welcomes community port links and discussion of merging ports. That is positive evidence of openness to downstream projects; the post does not specify source/binary license terms or contributor-rights scope. A scoped permission question has been drafted but not sent.
