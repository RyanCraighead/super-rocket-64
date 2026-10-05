#ifndef SMN64_DAMAGE_H
#define SMN64_DAMAGE_H
#include "combat_n64.h"
#include "../web/resource_n64.h"
typedef struct SmN64DamageState {
    SmN64Anim anim;
    uint32_t state,immune,input_disabled,scripted,invulnerable_ticks;
    uint8_t suit;
    int16_t health;
    uint32_t armor_active,armor_model_attached;
    int32_t armor;
    uint32_t surface_mode,wall,ceiling,aiming,turn_lock;
    uint32_t held_actor,web_graphic,swing_graphic,air_landing_wait;
    int32_t elapsed_ticks,position[3],velocity[3],normal[3],forward[3],up[3];
    uint32_t damage_tick,prior_damage_state,last_voice_tick;
    uint32_t stun,stun_ticks,movement_blocked;
    uint32_t combo_metric,display_timer,display_value;
} SmN64DamageState;
typedef struct SmN64DamageEvent {
    uint32_t stop_trails,drop_actor,release_web,release_swing;
    uint32_t exit_aim,unlock_camera,restore_armor_model,clear_armor_ui;
    uint32_t align_normal,alignment_forced;
    int32_t alignment_forward[3];
    uint32_t stun_effect,rumble_kind,sound,extra_sound,voice;
    uint32_t died;
} SmN64DamageEvent;
typedef struct SmN64DamageHost {
    void *context;
    /* Original800AA364 line-of-sight query for type8 knockback. */
    int (*line_clear)(void *,const int32_t from[3],const int32_t to[3]);
} SmN64DamageHost;
/* Original9746C scalar/gameplay fields with explicit actor, graphics, basis and
 * camera effects. Basis callbacks must honor invalid-basis source fallback.
 * Return0immune/rejected,1accepted,-1invalid,-2missing contact dependency. */
int smn64_player_damage(SmN64DamageState *,const SmN64CombatHit *,uint32_t tick,
    uint32_t rng[3],const uint16_t *counts,size_t,const SmN64DamageHost *,SmN64DamageEvent *);
/* Original97B00 argument0: death reaction. Scripted/gameflow argument1 is a
 * separate level-controller boundary, intentionally not implicitly executed. */
int smn64_player_die(SmN64DamageState *,const uint16_t *counts,size_t,SmN64DamageEvent *);
/* Original dead-state91A40: counts ticks only once the death clip is finished;
 * requests level/gameover state2 when the PRE-update timer is >=120. */
int smn64_death_wait(int32_t *wait_ticks,uint8_t animation_finished,int32_t elapsed_ticks);

#endif
