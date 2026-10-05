#ifndef SMN64_HURT_OWNER_H
#define SMN64_HURT_OWNER_H
#include "combat_n64.h"
/* Shared player fields, not a separate actor. Feed animation AFTER generic
 * authored successor selection and pass its completed clip (FFFF if none). */
typedef struct SmN64HurtOwner {
    SmN64Anim anim;
    uint32_t state,fall_latch_664,glove_hits,glove_display,glove_tick;
    uint32_t invulnerable_ticks,cf8,d20,field_660,jump_variant;
    uint16_t collision,fall_timer_1188,motion_speed;
    int16_t health;
    int8_t axis_1123,axis_1124;
    uint8_t jump_pressed,jump_latch_311;
    int32_t position_y,fall_origin_y,fall_min,fall_max,fall_damage_scale;
    uint32_t alternate_landing_sound,rumble_enabled;
} SmN64HurtOwner;
typedef struct SmN64HurtEvent { uint32_t sound,level_state,rumble; } SmN64HurtEvent;
typedef struct SmN64HurtHost {
    void *context;
    int (*stop)(void *,SmN64HurtOwner *); /*original97DE4, exactly1 success*/
    int (*die)(void *,SmN64HurtOwner *); /*original97B00(player,0)*/
    /* Actual source virtual damage packet: flags4 and u16 amount ONLY. Other
     * packet fields are not defined by9AD3C. Must update health synchronously. */
    int (*fall_damage)(void *,SmN64HurtOwner *,uint16_t amount);
} SmN64HurtHost;
/* Original8CBB0 prepass, once before inventory/physics/AI. Zero stays zero;
 * otherwise subtract elapsed as a word and clamp a negative signed result. */
uint32_t smn64_hurt_invulnerability_tick(uint32_t current,int32_t elapsed);
/* Original8D4DC airborne timeout prepass;1188 wraps asu16, resets unless state
 * has800000 or4 and collision lacks grounded bit2. */
uint16_t smn64_hurt_fall_timer_tick(uint16_t current,uint32_t state,
                                  uint16_t collision,int32_t elapsed);
/* Complete scalar state800000 continuation8DC7C–8DDD8, including knockdown
 * landing/recovery, 30-tick invulnerability, gloved-hands timing and death.
 * State8 recovery is owned by the common landing owner, not this function.
 * Errors may follow source state/RNG/callback changes: fail-stop or whole-owner
 * rollback. No invented stop, death, or damage response is substituted. */
int smn64_hurt_owner_step(SmN64HurtOwner *,uint16_t completed_animation,uint32_t tick,
    const uint16_t *counts,size_t,const SmN64HurtHost *,SmN64HurtEvent *);
/* Original9AD3C landing transition, including175/176→178 hurt landing, source
 * fall-damage math and exact jump/landing clip selection. Return0 no ground,
 * 1 landed (possibly killed by fall damage),-1 invalid,-2 missing callback.
 * The original falling state invokes die after this if health was already<=0;
 * callers retain that outer-state policy. Sound events may be ignored for silent
 * hosts, but alternate landing sound still advances the shared original RNG. */
int smn64_hurt_land(SmN64HurtOwner *,uint32_t rng[3],const uint16_t *counts,size_t,
    const SmN64HurtHost *,SmN64HurtEvent *);
#endif
