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

The current PC build retains `alBnkfNew.c` because it also provides the required `alSeqFileNew` function; its exact CC0-source match is documented. The seven old matrix/vector implementation files have project-authored replacements. Sanitizer tests and 50,000 matrix / 10,000 vector comparisons pass. The previously replaced reflection math passed 10,000 camera-view comparisons. These changes do not grant rights in unrelated source or assets.

## Exact resource groups still lacking a complete setup recipe

All 107 generated image inputs are pinned by path, size and hash to coopdx revision `8cd6e5977d9f920d51ca71f2c61801d019ed79c6`. The resource manifest supplies provenance but no per-image creator grant. The required groups are:

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

Four further 2,048-byte textures, `apparition_texture_1` through `_4` from `src/pc/apparition.inc.c`, supply a date-dependent visual override. No supported-source reconstruction recipe or scoped grant was identified for them. The separate 9,880-byte DejaVu-derived wheel font has its own retained font notice; it does not license the eight atlases above.

The two Bowser-key images and two custom castle images were searched in the supported US SM64 ROM and all 76 indexed MIO0 groups, including RGBA16 byte-order checks. None matched. Both custom castle images match each other but differ from the original castle-grounds texture slot. The other 103 images were not individually ROM-searched. These results establish **no verified reconstruction recipe**, not mathematical impossibility or evidence that a different game ROM is required.

Custom voice inputs are under `sound/samples/sfx_custom_{luigi,luigi_peach,wario,wario_peach,toad,toad_peach}`. The 123 present AIFFs are attributed in credits to Andrat (Luigi), Dark the Eagle (Wario) and Ninten_King_64 (Toad). Credits are not a redistribution grant, and these recordings must not automatically be described as Nintendo originals. Audio buffers also contain generated bank metadata, control bytecode and placeholders; their entire sizes must not be equated with recorded audio.

For preserving the exact custom artwork/voices, the missing evidence is a scoped license/permission record covering the pinned inputs and their underlying sources. Otherwise those resources need replacement with owned-ROM-derived or separately cleared material. No additional user ROM is currently justified as a solution to these custom-resource gaps.

## Structural data and work completed

Actor/level `model.inc.c` display lists and remaining vertices, `geo.inc.c` layouts, level scripts/collisions/placements/moving-texture tables, and `src/goddard/dynlists` data remain in inherited source. The inventory counted 4,047 actor/level model/display-list/vertex arrays and 34 Goddard dynamic-list/animation arrays. Source presence does not prove that each reaches the executable. Base-game data already available in the supported ROM is an extraction/host-format adaptation task; it should not be mislabeled as a missing user input. Later custom models and modifications require their own source/grant or replacement decision.

Four Mario wing vertex arrays now reconstruct directly from the supported US ROM: 26 vertices / 416 bytes. The actual C loader passes sanitizer-backed exact-hash checks while preserving the inherited white-opaque shading adjustment. No additional input or embedded vertex payload is required for those arrays.

The completed 115-buffer migration remains intact: 40,239,904 bytes of inventoried graphics/audio were removed from static initializers. The prior migration EXE contained none of those full byte sequences and passed early missing-data/tamper/repair probes. Its private folder is still not reproducible solely from the supported user inputs. Neither that folder nor any engine binary is published, and the launcher is not yet integrated with a complete cleared source for it.

## Remaining release decisions

1. Establish the grant for later fork contributions, with the documented CC0 baseline kept separate.
2. Preserve exact custom art/voices only with scoped provenance and permission evidence, or choose replacements. That choice may change appearance and voice presentation; the seven-character wheel and gameplay remain requirements.
3. Finish ROM-based reconstruction of required base-game structural data and any verified resource transforms. Preserve engine behavior through tests.
4. Retain the actual component notices and corresponding-source obligations for SDL, GLEW, RocketSim/Bullet, UE Viewer, CPython/dependencies, Unicorn, and the embedded libraries/font. Their licenses do not extend to unrelated game content.
5. Complete coordinated launcher UI, fresh-machine, gameplay/controller and matching-build Mario/Octane two-PC acceptance. Optional characters remain offline only.
