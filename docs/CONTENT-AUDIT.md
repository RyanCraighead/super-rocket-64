# Content and source audit

The public repository contains project documentation, creator-authorized gameplay GIFs, and standalone launcher/test source. It has fresh public history. It does not contain the game engine, a playable executable, ROMs, decoded character assets, saves, or a private release archive.

The GIFs depict proprietary games. Permission to reuse these recordings is not a license to redistribute their underlying game assets. The standalone launcher is project-authored; no downstream reuse license has been selected for it. Component credits do not grant a blanket license to the engine.

## What still blocks a game release

The integrated engine baseline records SM64coopdx revision `8cd6e5977d9f920d51ca71f2c61801d019ed79c6`. Its provenance documents do not identify a blanket repository license. The following inherited content requires a scoped resolution:

| Category | Concrete locations | Required resolution |
| --- | --- | --- |
| Static audio | `sound/sound_data.c`; compressed instrument/sample/sequence/bank data; custom Luigi, Toad and Wario voice inputs under `sound/samples` | Derive supported data from the user's local source, supply cleared replacements, or document redistribution permission for each remaining input. |
| Static graphics | Generated texture includes consumed by actor models, `bin/custom_textures.c`, and other engine translation units; four inline textures in `src/pc/apparition.inc.c` | Inventory the bytes actually linked, then remove the payloads from the distributed source and EXE or establish specific rights. |
| Structural game data | Geometry, vertex/display-list, animation and Goddard data in inherited source | A ROM file prompt alone does not remove these tables or resolve their provenance. |
| Restrictive library source | `lib/src/guLookAtRef.c`; separately, `lib/src/os.h` | The former was compiled into the first local Windows build and carries an explicit SGI restriction. A replacement is being verified locally. Inclusion of `os.h` in that Windows binary has not been established. Neither file is published here. |
| Unresolved library grants | `alBnkfNew.c`, `guMtxF2L.c`, `guNormalize.c`, `guOrthoF.c`, `guPerspectiveF.c`, `guRotateF.c`, `guScaleF.c`, `guTranslateF.c`, `ldiv.c` under `lib/src` | Trace the actual source and applicable grants, or replace the required implementation. Absence of a found grant is an unresolved question, not a finding of infringement. |

Moving unresolved bytes into an external asset pack does not make that pack redistributable. The private migration test pack is not available here and is not yet reproducible solely from the supported user game inputs. No future public installer may depend on it or private repository access.

The first local Windows binary contained exact byte matches for all 107 generated image arrays (33,923,328 bytes): 67 actor textures, 8 font atlases, 29 HUD/menu textures, one logo, and two level textures. It also contained four 2,048-byte inline apparition textures. Four audio buffers occupy a further 6,308,384 bytes. These inventories identify concrete payloads; they are not a claim that every listed image has the same owner or license.

The source scan also records 4,047 actor/level model, display-list and vertex arrays, plus separate geometry layouts, collisions, placements, scripts and other tables. It identifies 34 Goddard dynamic-list/animation arrays in 13 files. Source presence and a static initializer are evidence requiring review, not proof that every array reaches the final binary. The wheel's 9,880-byte DejaVu-derived raster has a separate scoped font notice.

A project-authored replacement for the reflection camera math passes sanitizer tests and comparison against 10,000 ordinary camera views. The PC build no longer selects `guLookAtRef.c`. The original file and its notice remain preserved privately. This resolves that implementation dependency in the candidate; it does not license the rest of the inherited source or headers.

## Scoped dependency terms

The development build uses components with separate notices: SDL's zlib-style terms; GLEW's combined GLEW/Mesa/Khronos notices; RocketSim MIT; Bullet zlib; UE Viewer MIT; CPython and its incorporated dependencies; and Unicorn's GPLv2 engine with separately licensed Python-binding metadata. Other retained source notices include Lua, stb, miniaudio, ini/mini, miniz, and the DejaVu-derived wheel font. The complete applicable notices and corresponding-source obligations must be checked against the exact release payload. None of those grants licenses Nintendo, Psyonix, Rare, Neversoft or other game content.

The current public launcher preview does not bundle those runtimes or libraries. The component inventory and local tests are evidence for continuing development, not legal clearance or a playable-release certification.
