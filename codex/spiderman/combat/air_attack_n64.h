#ifndef SMN64_AIR_ATTACK_H
#define SMN64_AIR_ATTACK_H
#include "combat_n64.h"
typedef struct SmN64AirAttack {
    SmN64Anim anim;
    uint32_t state, aiming, target_id, attack_tick, previous_attack_tick;
    uint32_t hit_done, landing_wait, movement_enabled, movement_blocked;
    int32_t position[3], previous_position[3], target_position[3], velocity[3];
    int32_t normal[3], horizontal_back[3], forward[3];
    uint32_t turn_a, turn_b;
} SmN64AirAttack;
typedef struct SmN64AirEvent { uint32_t detach_swing, face_target, align_normal, start_trails, stop_trails; } SmN64AirEvent;
/* Uses source target type gates and normalized Y >1024. Positions fixed12/Ydown.
 * No attack is created without an explicit live host target. */
int smn64_air_attack_request(SmN64AirAttack *,uint32_t target_id,uint16_t target_type,
    const int32_t target[3],uint8_t punch_pressed,uint8_t kick_pressed,uint32_t tick,
    const uint16_t *counts,size_t,SmN64AirEvent *);
/* Standard ongoing/landing path0x800916A0. Recent damage interruption must be
 * routed through the separate damage state owner before this function. */
int smn64_air_attack_tick(SmN64AirAttack *,uint8_t grounded,
    const uint16_t *counts,size_t,SmN64AirEvent *);
typedef struct SmN64AirHost {
    void *context;
    /* Query first actual actor contact in source-list order, excluding skip_id.
     * Return0 no hit,1 hit,-1 unavailable. policy comes from ROM bank by type. */
    int (*body_sweep)(void *,const int32_t from[3],const int32_t to[3],int32_t radius,
                     uint32_t skip_id,uint32_t *actor,uint16_t *type,int32_t contact[3]);
    int (*apply)(void *,const SmN64CombatHit *);
} SmN64AirHost;
int smn64_air_attack_contacts(SmN64AirAttack *,const SmN64CombatBank *,uint8_t suit,
    int32_t difficulty,const SmN64AirHost *);
#endif
