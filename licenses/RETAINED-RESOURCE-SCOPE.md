# Finite retained-resource decision for the baseline

This edition retains the original custom resources identified below, with upstream attribution and the component terms. The provenance record distinguishes inherited source grants, explicit component licenses and upstream distribution practice. No universal MIT/CC0 grant from every later contributor is asserted.


## Exact resources

The 107 PNGs are enumerated with source paths and hashes in `codex/windows/engine_recipe.json`, under `source_png`; their packaged originals are in `codex/windows/visual_inputs/`:

| Original upstream directory | PNG count |
|---|---:|
| `actors/bowser_key` | 2 |
| `actors/luigi` | 7 |
| `actors/mario` | 12 |
| `actors/toad_player` | 13 |
| `actors/waluigi` | 16 |
| `actors/wario` | 17 |
| `levels/castle_courtyard` | 1 |
| `levels/castle_grounds` | 1 |
| `textures/custom_font` | 8 |
| `textures/segment2` | 30 |

Four further textures are the symbols `apparition_texture_1` through `apparition_texture_4` in the single original source file `src/pc/apparition.inc.c`, preserved as `codex/windows/visual_inputs/src/pc/apparition.inc.c`. Its SHA-256 is `b137170ede765ed8d3b364c760e137c7f9e18f4eb8163c72997b86e42324a691`. They are explicitly source-defined custom texture data, not something the user must provide from a different game.

The 123 original AIFFs and their derived encodings are listed individually in `codex/windows/audio_inputs/encoding-manifest.json`. Each of Luigi, Toad and Wario has 27 files in `sound/samples/sfx_custom_<character>/` and 14 in `sound/samples/sfx_custom_<character>_peach/`: 41 per character. Upstream `credits.txt` names Andrat for Luigi, Dark the Eagle for Wario and Ninten_King_64 for Toad. The `_peach` directory suffix does not identify an additional game's extracted audio or require another user input. These recordings should not be labelled original Nintendo samples without evidence. Attribution remains intact.

The later source-contribution scope comprises the used fork additions in `src/pc/network/` (including `network_player.c`), `src/pc/lua/`, `src/pc/djui/`, DynOS code under `data/`, and their integrated gameplay changes. These are source changes, not a new proprietary-asset category.

## Existing evidence and concrete terms

- The [original decomp CC0 adoption](https://github.com/n64decomp/sm64/commit/66018e9f3caaa67399218971d61366cb3f7ba7d7) and prior-collaborator evidence cover inherited contributions; the prior lineage review found 956 identical same-path blobs. A later fork omitting a root notice does not erase inherited grants. This does not invent a blanket CC0 grant from later contributors.
- Upstream gave [explicit Flathub approval](https://github.com/coop-deluxe/sm64coopdx/issues/1112#issuecomment-4413172616) and [invites community ports](https://github.com/coop-deluxe/sm64coopdx/discussions/1224). Those are positive distribution/derivative-practice evidence. They are not falsely relabelled a universal MIT/CC0 resource license. Their scope is recorded here without presenting either as a universal resource license.
- The [updated Emill/MaikelChan renderer terms](https://github.com/Emill/n64-fast3d-engine/blob/881eb68bad1150f720433d75bccb4d26879c28ab/LICENSE.txt) permit source redistribution with notice and condition binary redistribution on rights to included assets. The obsolete absolute binary ban must not be applied. The supported ROM, extracted game profiles and game-package payloads stay user-supplied; custom upstream resources remain separately identified above.
- The actual adverse SGI wording was confined to the identified original `lib/src/guLookAtRef.c` implementation and `lib/src/os.h`. The former was replaced with project-authored PC math; the latter is excluded. Neither implementation is in the clean export. No similarly explicit adverse term was identified for the 107 PNGs, apparition arrays or 123 custom recordings. Missing individual notices is not contrary evidence.
- Retain applicable original-decomp, renderer, Lua, SDL2, GLEW, zlib, miniz, RocketSim/Bullet, MinGW/GCC, embedded CPython, Unicorn and extraction-tool notices. Unicorn corresponding source remains packaged. `DISCORD_SDK=0`, `COOPNET=0`, `UPDATER=0`: no obligations are invented from external libraries not linked or shipped.

The exact retained-file manifest accompanies the source and Windows package. Upstream attribution and applicable component terms remain included.
