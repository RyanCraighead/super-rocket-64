#ifndef SMN64_WEB_ABILITIES_H
#define SMN64_WEB_ABILITIES_H
#include "combat_n64.h"
#include "command_n64.h"
#include "../web/resource_n64.h"
/* Original logical axes are retained verbatim. The platform controller mapper
 * must establish the manual's physical direction mapping rather than treating
 * these source bytes as an ordinary screen X/Y stick. */
typedef struct SmN64WebAbility {
    SmN64Anim anim;
    uint32_t state, latched, moving_latch, blocked_directions;
    uint32_t holding_object, surface_mode, aiming, ceiling_orientation;
    int32_t run_ramp;
    int8_t axis_1123, axis_1124;
    uint8_t attack_mode, actor_flags;
    uint32_t fired, aim_snapshot, yank_variant;
    uint32_t last_glove_tick, glove_hits, glove_fade, dome_tick;
} SmN64WebAbility;
typedef struct SmN64WebButtons { uint8_t web, punch, kick, jump; } SmN64WebButtons;
typedef struct SmN64WebAbilityEvent {
    uint32_t create_dome, destroy_dome, grab_target_query, face_target;
    uint32_t glove_sound, fire_amount, fire_kind, release_web;
    SmN64WebResourceEvent resource;
} SmN64WebAbilityEvent;
/* Pure source99444 latch/direction selector. prepared contains ONLY the
 * original request prelude. No animation lookup, inventory, RNG or callbacks
 * run here. command/animation describe the exact selected branch, including
 * a command whose subsequent original resource check would fail. */
typedef struct SmN64WebAbilityPlan {
    SmN64WebAbility prepared;
    SmN64CombatCommand command;
    uint16_t animation;
} SmN64WebAbilityPlan;
int smn64_web_ability_plan(const SmN64WebAbility *,const SmN64WebButtons *,
    uint32_t tick,SmN64WebAbilityPlan *);
/* Exact request boundary80099444: animation/resource gates + semantic host
 * events for dome allocation and target handling. No invented projectile hits.
 * Return1 accepted,0 rejected,-1 malformed dependency. */
int smn64_web_ability_request(SmN64WebAbility *, const SmN64WebButtons *, uint32_t tick,
    SmN64WebResource *, uint32_t rng[3], const uint16_t *counts, size_t,
    SmN64WebAbilityEvent *);
/* Source scalar glove285 and dome283/284 state handlers. The caller must report
 * the generic source animation-successor update BEFORE these handlers, and
 * pass its completed clip (0xffff if none). Dome construction/contact damage remains host
 * actor work; events do not imply a target was hit. */
int smn64_web_gloves_tick(SmN64WebAbility *,uint16_t completed_animation,uint32_t tick,
    const uint16_t *counts,size_t,SmN64WebAbilityEvent *);
int smn64_web_dome_tick(SmN64WebAbility *,const SmN64WebButtons *,uint16_t completed_animation,uint32_t tick,
    const uint16_t *counts,size_t,SmN64WebAbilityEvent *);
#endif
