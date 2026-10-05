#ifndef SM64_SPIDERMAN_EFFECT_SCENE_H
#define SM64_SPIDERMAN_EFFECT_SCENE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "../../codex/spiderman/web/lifecycle_n64.h"
#include "../../codex/spiderman/combat/trails_n64.h"
#include "../../codex/spiderman/controller/web_attack_effects_n64.h"
#include "../../codex/spiderman/combat/dome_shatter_n64.h"
int spiderman_effect_scene_init(const char *original_asset_directory);
int spiderman_effect_scene_enable_dome(const char *original_dome_asset_directory);
int spiderman_effect_scene_dome_ready(void);
int spiderman_effect_scene_ready(void);
void spiderman_effect_scene_shutdown(void);
void spiderman_effect_scene_suspend(void);
/* Copy only after the complete source/host transaction commits. Pointer-free
 * creation serial metadata shares the supplied transaction clock. Traversal
 * and attack entries may be interleaved; trails are supplied newest-first.
 * Bounds:32 strands/splats,512 attack objects,32 trails. NULL permits zero
 * objects; NULL visuals represents an empty traversal registry. */
int spiderman_effect_scene_submit(const SmN64WebVisuals *,
    const SmN64WebAttackObject *,size_t attacks,const SmN64Trail *,size_t trails,
    uint32_t source_tick,uint64_t shared_graphical_clock);
/* Add the same committed frame's original shatter polygons after submit.
 * Source F5540 is merged newest-first with actual decals/splats. Legacy submit
 * clears this optional registry; failure invalidates the complete frame. */
int spiderman_effect_scene_submit_dome_shards(const SmN64DomeShatterFragment *,
    size_t count,uint32_t source_tick);
/* Initial ENV is captured from the explicitly selected caller render boundary
 * AT DRAW TIME, not guessed from source lifetime or ambient GL state. The native
 * embedding uses authoritative host SM64 RDP ENV alpha. That is a cross-game
 * caller input, not an assertion about an original Spider-Man campaign frame.
 * Original source FB writes are replayed in primitive order. Optional finalENV
 * is published only on success. Draw never mutates gameplay or source clocks. */
int spiderman_effect_scene_draw(const float view[16],const float projection[16],
    const int viewport[4],uint8_t initial_environment_alpha,uint8_t *final_environment_alpha);
typedef struct SpidermanEffectDrawCounts {
    uint32_t source_tick;
    uint32_t traversal_knot_quads,traversal_splat_quads,trail_quads;
    uint32_t traversal_strand_chains,attack_quads,attack_lines;
    uint32_t dome_shatter_triangles;
} SpidermanEffectDrawCounts;
/* Diagnostics of the last successfully drawn committed snapshot, never copied
 * into old standalone scene counters. Each submit/suspend/failure clears them.
 * Returns1 after successful draw (including empty), otherwise0 and zero fields.
 * A strand contributes TWO source chains; fragment contributes TWO source lines.
 * Counts describe submitted source primitives, not nonzero framebuffer pixels. */
int spiderman_effect_scene_draw_counts(SpidermanEffectDrawCounts *);
const char *spiderman_effect_scene_status(void);
unsigned spiderman_effect_scene_drawn_quads(void);
unsigned spiderman_effect_scene_drawn_strand_chains(void);
unsigned spiderman_effect_scene_drawn_lines(void);
#ifdef SPIDERMAN_TESTING
typedef struct SpidermanEffectOrderProbe {
    uint32_t source_list;
    uint64_t serial; /* trail-only ordinal; shared allocation serial otherwise */
    uint32_t primitive_kind,texture_slot;
} SpidermanEffectOrderProbe;
/* Pure preparation diagnostic; no GL calls or retained source updates. */
int spiderman_effect_scene_test_order(const float view[16],SpidermanEffectOrderProbe *,size_t capacity,size_t *count);
#endif
#ifdef __cplusplus
}
#endif
#endif
