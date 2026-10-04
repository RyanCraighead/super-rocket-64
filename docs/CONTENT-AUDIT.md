# Content and source audit

No playable public release is available. This page distinguishes explicit restrictions, documented grants, missing documentation, and unfinished local extraction work. A missing grant is not a finding of infringement.

## What is public

Main contains the README, setup/maintenance/release documentation and two creator-authorized gameplay GIFs. Draft PR #1 adds project-authored standalone launcher/test source and this audit. No engine source, ROM, decoded character data, executable, save, private package or private Git ancestry is included.

The only third-party game imagery in that tree is in `docs/media/castle-flight.gif` and `docs/media/battlefield-jumps.gif`. The owner authorized these recordings. That authorization does not grant rights in the underlying raw game assets. No third-party implementation was identified in the standalone launcher. Its downstream reuse license is undecided; that does not block the owner's authorized publication of this source preview.

## Source grants and restrictions

The original [n64decomp/sm64 repository adopted CC0 on 2023-01-09](https://github.com/n64decomp/sm64/commit/66018e9f3caaa67399218971d61366cb3f7ba7d7). Its [license](https://github.com/n64decomp/sm64/blob/66018e9f3caaa67399218971d61366cb3f7ba7d7/LICENSE.md) covers rights held by the affirmers and expressly does not clear other people's rights. This corrects the earlier overly broad description of the inherited source as lacking identified grant evidence.

| Component | Evidence and present status |
| --- | --- |
| Original decomp contributors' source | Documented CC0 grant. Preserve its scope and notice; do not extend it to third-party game content or later fork additions. |
| Seven baseline library files | `alBnkfNew.c`, `guNormalize.c`, `guOrthoF.c`, `guPerspectiveF.c`, `guRotateF.c`, `guScaleF.c`, and `guTranslateF.c` exactly match files at the CC0 commit. They are not simply “no grant found.” |
| Two modified library files | `guMtxF2L.c` and `ldiv.c` differ from that snapshot. Their later deltas lacked a verified scoped grant. The candidate now uses project-authored matrix adapters and excludes `ldiv.c`. |
| SGI reflection implementation | `lib/src/guLookAtRef.c` contains an explicit restriction on disclosure/copying without written consent. It is replaced in the PC build and must be excluded from a future public source export. The original remains preserved privately. |
| Separate SGI header | `lib/src/os.h` has an explicit restriction too. It was not found in the inspected PC dependency graph. Exclude it from a source export; there is no evidence here that it is in the PC EXE. |
| Later PC/multiplayer fork changes | Pinned coopdx `src/pc/network/network_player.c` is absent from the original CC0 tree. `src/game/mario.c`, `src/game/level_update.c`, and `src/engine/math_util.c` differ from it. A grant for the later contributions has not been established by the inspected README, credits or root notices. The next evidence needed is a scoped maintainer/contributor grant for those fork layers, not another ROM. |

The multiplayer file can now be traced to [MysterD's 2020-09-19 addition](https://github.com/sm64pc/sm64ex/commit/f8bffd3b2a04830d30e1be8b5ba727fd89713f7c) in the PC fork, with the same commit retained in the coop and coopdx histories. Checked notices at that origin and the current fork refs cover specific tools/dependencies; no applicable project-wide or per-file grant was located for this network file or the inspected later game/math changes. This narrows the provenance gap without establishing either infringement or clearance.

The current PC build retains `alBnkfNew.c` because it also provides the required `alSeqFileNew` function; its exact CC0-source match is documented. The seven old matrix/vector implementation files have project-authored replacements. Sanitizer tests and 50,000 matrix / 10,000 vector comparisons pass. The previously replaced reflection math passed 10,000 camera-view comparisons. These changes do not grant rights in unrelated source or assets.

## Exact resource groups still lacking a complete setup recipe

All 107 generated image inputs are pinned by path, size and hash to coopdx revision `8cd6e5977d9f920d51ca71f2c61801d019ed79c6`. The resource manifest supplies provenance. Individual image notices are not required if an applicable repository-wide grant covers the contribution. At this exact upstream revision, the root tree and README contain no located blanket grant; the MIT notices found are scoped to tools/dependencies. That repository-level gap must not be confused with merely missing per-image notices. The required groups are:

| Location | Inputs |
| --- | ---: |
| `actors/bowser_key/bowser_key_{left,right}.rgba16.png` | 2 |
| `actors/luigi/custom_*.png` | 7 |
| `actors/mario/custom_*.png` | 12 |
| `actors/toad_player/custom_*.png` | 13 |
| `actors/waluigi/custom_*.png` | 16 |
| `actors/wario/custom_*.png` | 17 |
| `levels/castle_grounds/6_custom.rgba16.png`, `levels/castle_courtyard/0_custom.rgba16.png` | 2 |
| `textures/custom_font/custom_font_{aliased,hud,hud_recolor,jp,jp_aliased,normal,special,title}.rgba32.png` | 8 |
| `textures/segment2/`: coopdx logo, extra HUD glyphs, character heads, ping/selection icons and spike shadow | 30 |

Four further 2,048-byte textures, `apparition_texture_1` through `_4` from `src/pc/apparition.inc.c`, supply a date-dependent visual override. They now reproduce exactly from the preserved pinned upstream source arrays; no ROM-only route is claimed. Their applicable grant follows the scoped source review. The separate 9,880-byte DejaVu-derived wheel font has its own retained font notice; it does not license the eight atlases above.

The two Bowser-key images and two custom castle images were searched in the supported US SM64 ROM and all 76 indexed MIO0 groups, including RGBA16 byte-order checks. None matched. Both custom castle images match each other but differ from the original castle-grounds texture slot. The other 103 images were not individually ROM-searched. These results establish **no verified ROM-only reconstruction recipe**, not mathematical impossibility or evidence that a different game ROM is required.

Custom voice inputs are under `sound/samples/sfx_custom_{luigi,luigi_peach,wario,wario_peach,toad,toad_peach}`. The 123 present AIFFs are attributed in credits to Andrat (Luigi), Dark the Eagle (Wario) and Ninten_King_64 (Toad). Credits are not a redistribution grant, and these recordings must not automatically be described as Nintendo originals. Audio buffers also contain generated bank metadata, control bytecode and placeholders; their entire sizes must not be equated with recorded audio.

An independent serializer now reconstructs the 160-byte bank-set table exactly from sequence/bank metadata, without ROM or sample bytes. The music buffer begins with a 16,832-byte prefix containing the 16,252-byte assembled sound-player program, generated sequence index and alignment. Its 34 music destinations are zero placeholders in the validated local pack; the existing loader fills 100,129 bytes from the supported ROM. Exact comparisons confirm that these ROM overlays determine the final sequence bytes. This resolves neither the later control-program source grant nor custom sample permissions. The 27 extra Peach references in the three custom voice banks are conditional on `EXTENDED_CHAR_SOUNDS`, which is disabled in the inspected build. They correspond by name to existing Mario/Peach ROM samples, so no additional user files are requested for them.

Assess any applicable repository-wide or component grant, contributor terms, explicit exclusions and third-party provenance before deciding that a resource needs removal or replacement. Missing individual asset notices, custom appearance and unused status are not sufficient reasons. A broad feature-removal attempt was withdrawn completely after this scope clarification; all affected assets and character functionality are restored in the private candidate. No additional user ROM is currently justified as a solution to an unresolved grant question.

## Structural data and work completed

Remaining static actor/level `model.inc.c` display lists and vertices, `geo.inc.c` layouts, level scripts and other unmapped tables remain outside the completed bounded reconstruction proof; the Goddard follow-up below resolves its literal geometry and command arrays. The 70 enumerated level object-placement and trajectory definitions now load from the owned ROM, as detailed below. The inventory counted 4,047 actor/level model/display-list/vertex arrays and 34 Goddard dynamic-list/animation arrays. Source presence does not prove that each reaches the executable. Base-game data already available in the supported ROM is an extraction/host-format adaptation task; it should not be mislabeled as a missing user input. Later custom models and modifications need the same repository/component-license and third-party provenance assessment; they do not automatically require individual permissions or replacement.

Four Mario wing vertex arrays now reconstruct directly from the supported US ROM: 26 vertices / 416 bytes. The actual C loader passes sanitizer-backed exact-hash checks while preserving the inherited white-opaque shading adjustment. No additional input or embedded vertex payload is required for those arrays.

The actual C ROM loader now reconstructs 42 object-placement tables, 28 movement-path tables and 851 base-game light structs: 921 structures / 39,764 bytes. Their output exactly matches the previous compiled definitions. Sanitizer tests cover zero initialization, native-endian signed words, complete light layouts and 18 invalid-span cases with unchanged outputs. The Windows build and early data probe pass; all 921 selected symbols occupy uninitialized storage in the linked EXE. Sixty unmatched light definitions remain unchanged, and custom player actors were excluded from the light batch. This work uses the existing supported SM64 ROM and changes no presentation values.

The completed 115-buffer migration remains intact: 40,239,904 bytes of inventoried graphics/audio were removed from static initializers. The prior migration EXE contained none of those full byte sequences and passed early missing-data/tamper/repair probes. The current recipe now regenerates its shared files from the supported user ROM and original/source-built components; it has no private full-buffer seed dependency. Neither that folder nor any engine binary is published. The actual launcher now assembles and validates the full private profile, including owned-ROM audio and generated bank metadata; the original and source-built components remain private pending the scoped grant and export review.

## Remaining release decisions

1. Assess the used fork-specific contributions against the actual upstream lineage and community grant evidence, keeping inherited CC0-covered source and component grants separate from later changes.
2. Apply any established repository/component grant to original custom art and voices within its scope. Preserve the required presentation and seven-character wheel; do not demand individual notices solely because no per-file license exists, and do not remove resources as a shortcut.
3. Finish ROM-based reconstruction of required base-game structural data and any verified resource transforms. Preserve engine behavior through tests.
4. Retain the actual component notices and corresponding-source obligations for SDL, GLEW, RocketSim/Bullet, UE Viewer, CPython/dependencies, Unicorn, and the embedded libraries/font. Their licenses do not extend to unrelated game content.
5. Complete coordinated launcher UI, fresh-machine, gameplay/controller and matching-build Mario/Octane two-PC acceptance. Optional characters remain offline only.

## Updated lineage, dependency and setup evidence

The source review now follows the actual lineage through SM64coopdx, the pre-deletion `djoslin0/sm64ex-coop` history, `sm64pc/sm64ex`, `sm64-port`, and n64decomp. It keeps inherited CC0-covered source separate from later fork-specific contributions. A missing root license is not a blanket prohibition.

The renderer's [newer upstream license](https://github.com/Emill/n64-fast3d-engine/blob/881eb68bad1150f720433d75bccb4d26879c28ab/LICENSE.txt) permits binary redistribution when the binary contains no assets that the distributor lacks rights to distribute. Its required notice/conditions are retained in the private candidate; the stale inherited README's older binary prohibition is not treated as current. Upstream also has an explicit [Flathub distribution approval](https://github.com/coop-deluxe/sm64coopdx/issues/1112#issuecomment-4413172616), which is positive permission evidence for that proposal. Its scope is considered alongside the [community-port invitation](https://github.com/coop-deluxe/sm64coopdx/discussions/1224), not silently extended to every downstream project or third-party asset.

Our actual Windows build sets `COOPNET=0`. Resolved link flags contain neither `libcoopnet` nor `libjuice`, and the linked map contains none of the 12 external CoopNet API entries or libjuice transport symbols. The inherited fork's ID/compatibility helpers remain distinct. The upstream default alone therefore does not establish a static CoopNet/libjuice obligation for this build. Other enabled dependency terms still apply.

The current private Windows installer reproduces all 115 externalized shared runtime files from original source components, generated metadata and the existing owned-ROM input. It contains **zero private full-buffer seed files**. This resolves the technical source routes for all 113 former seed dependencies: 107 PNG conversions, four inline source textures, and two mixed source/ROM audio buffers. No additional user-selected game, recording, Python installation or extraction executable is required.

The standard-library visual converter reproduces all 111 visual outputs exactly. The music path uses a reproducible source-built sound-player prefix, zero-filled alignment gaps and 34 owned-ROM spans. A narrowly asserted build-stage source adjustment keeps the validated Toad note velocity at 127 instead of the source file's 100; the original source stays preserved. The sample path uses 123 recordings encoded during the project build, the existing bank metadata, and 297 owned-ROM spans. The complete music, sample and sound-control buffers all match their registered hashes. Compilers and audio encoders are maintainer build tools; they are not installed or executed on the user's Windows machine.

The exact v5 payload passed 71 setup/reuse/repair/cancellation/source-integrity/launch-wiring checks and 64 actual launcher UI checks on a separate, never-activated desktop. Six additional upgrade checks passed against a copy of the v4 data profile: no original input arguments were needed; save/controller/character bytes and timestamps, tool caches, the old shared cache and the original v4 profile remained unchanged. The engine EXE is unchanged, and its early probe validates all 115 generated buffers. No native gameplay or peer connection was started.

Source reproducibility is distinct from permission to publish original/custom resources or other embedded engine material. The scoped source/resource review remains unresolved. Ryan subsequently reported successful manual testing; broader controller and two-PC coverage is not inferred. The public preview still excludes the engine, source-resource pack and playable EXE. All earlier verified installers, source inputs, character features and presentation are preserved.

## Goddard reconstruction and current baseline

The earlier embedded face/eye finding is fixed. The actual C loader now reconstructs all 12 literal geometry arrays and all nine DynList command tables in the 13 Goddard dynlist source files from the same supported owned SM64 ROM. This covers 13,568 geometry bytes and 1,303 commands / 31,272 ROM bytes. All 63 host-pointer bindings are explicit. Complete native structures match the original definitions, including float bits, padding and pointers; sanitizer tests cover zero initialization and 21 rejected malformed command/binding/span cases with unchanged output. The 25 remaining animation descriptors contain counts, types and pointers to already ROM-loaded animation data.

The final Windows build places all 21 selected arrays in uninitialized storage, with zero file bytes for those symbols. The 12 original full geometry sequences are absent from the engine. No literal DynList initializers remain in those 13 source files. This is a specific verified result, not clearance of every other actor/level command table.

The separate v7 installer passed 71 setup, repair, cancellation, integrity and launch-wiring checks; 64 actual UI checks on a never-activated Windows desktop; and six copied-profile upgrade checks. The setup produces all 115 shared files at their expected hashes. Existing structural-table, light, scalar-display-list and wing-loader sanitizer regressions also pass. The package audit verifies all 569 manifest files and 1,862 entries including nested dependency archives. The v5 and v6 installers remain byte-identical to their preserved versions.

The package retains component notices for the actual linked dependencies, including the updated conditional renderer grant, original-decomp CC0 notice, SDL2, GLEW, zlib, RocketSim/Bullet, MinGW/GCC runtime terms and the exact MIT-style miniz notice. Existing CPython, Unicorn corresponding source, font and extraction-tool notices remain included. The actual final engine is built with `DISCORD_SDK=0 COOPNET=0 UPDATER=0`; its imports/map contain no Discord SDK, external CoopNet API or libjuice linkage. Fork-owned compatibility helpers are not SDK linkage.

Ryan reported that the v5 installer was working after manual testing. That success is accepted; no repeat test is requested. It does not specify all-character, physical-controller or two-PC coverage. The automated v7 checks did not start gameplay or network peers and did not take the foreground.

The remaining publication decision is the applicable grant for used later fork contributions (including `src/pc/network/network_player.c` and later Lua/DJUI/DynOS changes) and retained original custom resources: 107 PNG sources, four apparition textures and 123 credited custom voice recordings with their derived encodings. The original CC0 grant, component licenses, explicit Flathub approval and community-port invitation are positive scoped evidence; the current record does not establish their full coverage of this derivative and those resources. This is not a finding that a missing per-file notice forbids distribution. No feature/resource removal or external outreach is proposed. Other linked actor/level command tables also remain outside the completed bounded proof. The engine, source-resource pack and playable EXE remain private pending that exact scope decision and final export audit.

Updater and optional-shortcut work is paused for the second release. Neither is included in this baseline.
