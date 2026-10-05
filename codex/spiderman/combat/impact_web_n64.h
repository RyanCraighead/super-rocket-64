#ifndef SMN64_IMPACT_WEB_H
#define SMN64_IMPACT_WEB_H
#include "combat_n64.h"
#include "../web/resource_n64.h"
/* Original impact-web actor0x800B20A0/0x800B22F4. Fixed12/Y-down. */
typedef struct SmN64ImpactWeb {
    int32_t position[3],velocity[3];
    uint32_t previous_tick,damage;
    int16_t age,size,rotation,rotation_step;
    uint16_t life;
    uint8_t alive,world_hit;
    int32_t world_position[3];
    int16_t world_normal[3];
    uint32_t special_actor;
} SmN64ImpactWeb;
typedef struct SmN64ImpactWorldHit {
    uint8_t hit;
    int32_t distance,position[3];
    int16_t normal[3];
} SmN64ImpactWorldHit;
typedef struct SmN64ImpactEvent {
    uint32_t reason; /*0ongoing,1expiry,2actor contact,3pretraced world contact*/
    uint32_t actor,sound,activate_special,spawn_burst,spawn_world_decal;
    int32_t burst_position[3];
    int16_t decal_normal[3];
} SmN64ImpactEvent;
typedef struct SmN64ImpactHost {
    void *context;
    /* Source constructor query: full lifetime ray, environment flag0/actorlist0.
     * Do not replace a missing query with a no-hit result. */
    int (*world_trace)(void *,const int32_t from[3],const int32_t to[3],SmN64ImpactWorldHit *);
    /* Source800AA8DC: ordered actor swept query followed by its dynamic-object
     * line fallback. Return1 actual actor,0no contact,-1unavailable. */
    int (*actor_sweep)(void *,const int32_t from[3],const int32_t to[3],uint32_t *actor);
    int (*apply)(void *,const SmN64CombatHit *);
    int (*special_active)(void *,uint32_t actor);
} SmN64ImpactHost;
/* Angles are source pitch/yaw from800A8EEC; speed is native32 for ordinary
 * FireWeb. initial_damage50, lifetime30 for clip139 else120. */
int smn64_impact_web_init(SmN64ImpactWeb *,const int32_t muzzle[3],const int16_t angles[3],
    int32_t speed,uint16_t initial_damage,uint16_t lifetime,uint32_t special_actor,
    uint32_t tick,uint8_t player_present,uint8_t suit,int32_t difficulty,
    uint32_t rng[3],const SmN64ImpactHost *);
int smn64_impact_web_tick(SmN64ImpactWeb *,uint32_t tick,const SmN64ImpactHost *,SmN64ImpactEvent *);
#endif
