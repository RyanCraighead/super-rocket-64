#ifndef SM64_SPIDERMAN_WEB_ATTACK_SCENE_H
#define SM64_SPIDERMAN_WEB_ATTACK_SCENE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "../../codex/spiderman/controller/web_attack_effects_n64.h"
typedef struct SpidermanWebAttackRenderState {
    uint8_t environment_alpha;
    uint8_t environment_alpha_known; /* proof-bearing caller boundary; no default */
} SpidermanWebAttackRenderState;
/* Presentation only: no source time, updates, RNG, contacts or allocations into
 * the gameplay registry. init/ready prove exact resource availability, not that
 * a caller has recovered ENV for a burst or that a GL draw has succeeded. */
int spiderman_web_attack_scene_init(const char *original_asset_directory);
int spiderman_web_attack_scene_ready(void);
void spiderman_web_attack_scene_shutdown(void);
void spiderman_web_attack_scene_suspend(void);
/* Copies at most512 original objects in strict newest-ID-first order. Active
 * burst requires known explicit sourceENV. Zero count accepts NULL objects and
 * NULL state. Any failure hides the previous snapshot. No fallback geometry. */
int spiderman_web_attack_scene_submit(const SmN64WebAttackObject *,size_t count,
    uint32_t source_tick,const SpidermanWebAttackRenderState *);
/* Uses source320x240 widthfield0 line policy. Source sprite list first, decals
 * second, line list last; child fragment line precedes parent. One validated
 * shared draw frame; any failure invalidates the snapshot and clears stats. */
int spiderman_web_attack_scene_draw(const float view[16],const float projection[16],const int viewport[4]);
const char *spiderman_web_attack_scene_status(void);
unsigned spiderman_web_attack_scene_drawn_quads(void);
unsigned spiderman_web_attack_scene_drawn_lines(void);
#ifdef SPIDERMAN_TESTING
int spiderman_web_attack_scene_test_snapshot(SmN64WebAttackObject *,size_t capacity,size_t *count,uint32_t *source_tick);
#endif
#ifdef __cplusplus
}
#endif
#endif
