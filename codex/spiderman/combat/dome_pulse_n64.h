#ifndef SMN64_DOME_PULSE_H
#define SMN64_DOME_PULSE_H
#include "combat_n64.h"
typedef struct SmN64DomePulse {
    int32_t position[3],radius,fade[3],growth;
    uint32_t web_type;
    uint16_t rotations[16];
    uint8_t alive;
} SmN64DomePulse;
typedef struct SmN64DomeActor {
    uint32_t id,flags;
    uint16_t type;
    int32_t position[3];
} SmN64DomeActor;
typedef struct SmN64DomeHost {
    void *context;
    int (*actor_at)(void *,uint32_t list,size_t index,SmN64DomeActor *);
    int (*mark_hit)(void *,uint32_t actor,uint8_t set);
    int (*apply)(void *,const SmN64CombatHit *);
    /* Synchronous source visual/actor-special event boundary BEFORE damage.
     * kind1=enemy impact effect72298,kind2=special0x144 actor request(20,255,123).
     * unavailable effects must return-1, never silently consume arbitrary RNG. */
    int (*effect)(void *,uint32_t kind,uint32_t actor);
    uint8_t effect_pool_unavailable;
} SmN64DomeHost;
int smn64_dome_pulse_init(SmN64DomePulse *,const int32_t player_position[3],
    uint32_t web_type,uint32_t rng[3],const SmN64DomeHost *);
/* SourceB36F8+B34D4: growth140 per actor tick, RGB240→0 by16, source damage
 * 200-radius*200/500 throughradius500 then0, actual actor gates and once-hitbit.
 * The final fade tick still processes contacts, exactly as the source does. */
int smn64_dome_pulse_tick(SmN64DomePulse *,const SmN64DomeHost *);
#endif
