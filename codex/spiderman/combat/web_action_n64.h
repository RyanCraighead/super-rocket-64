#ifndef SMN64_WEB_ACTION_H
#define SMN64_WEB_ACTION_H
#include "web_abilities_n64.h"
#include "fireweb_n64.h"
typedef struct SmN64WebAction {
    SmN64WebAbility ability;
    int32_t position[3],forward[3],right[3],up[3],explicit_target[3];
    int16_t target_normal[3];
    uint32_t graphic,target,phase;
    uint16_t target_type,hold_timer;
} SmN64WebAction;
typedef struct SmN64YankCurve { uint8_t present;int32_t points[8][3]; } SmN64YankCurve;
/* Exact source9E554 eight-point over-shoulder path, including128unit vertical
 * eligibility, source wrapped distance, cosine radius shrink after each of first4 points. */
int smn64_yank_curve(const int32_t player[3],const int32_t target[3],SmN64YankCurve *);
typedef struct SmN64WebActionHost {
    void *context;
    int (*jump)(void *,SmN64WebAction *); /*1transitioned,0no jump,-1missing*/
    int (*stop)(void *,SmN64WebAction *); /*original97DE4 semantics*/
    int (*create_graphic)(void *,uint8_t hand,uint32_t web_type,uint32_t *id);
    int (*release_graphic)(void *,uint32_t id,uint8_t immediate_delete);
    int (*graphic_target)(void *,uint32_t graphic,uint32_t *actor);
    int (*target_position)(void *,uint32_t actor,int32_t out[3]);
    /* Synchronous FireWeb dispatch+effects: amount, initial-surface flag,
     * explicit normal. Must preserve original RNG effects before returning. */
    int (*fire)(void *,SmN64WebAction *,uint8_t auto_target,int32_t amount,uint8_t surface,
                const int16_t normal[3],uint32_t *result_flags);
    int (*yank)(void *,uint32_t graphic,const int32_t velocity[3],const SmN64YankCurve *);
    int (*impact_sparks)(void *,uint32_t count); /*source9F780:4 or2*/
    int (*dome)(void *,SmN64WebAction *,uint32_t web_type);
    int (*grab)(void *,SmN64WebAction *); /*source target query max190*/
} SmN64WebActionHost;
/* Complete player scalar trap/yank/impact handlers for states4000/8000/10000/
 * 20000. Called after generic original animation successor handling. Early
 * modifiers preserve source precedence. Global inventory/RNG shared with host.
 * Returns1handled,0different state,-1bad,-2missing/failed synchronous dependency. */
int smn64_web_action_step(SmN64WebAction *,const SmN64WebButtons *,uint32_t tick,
    SmN64WebResource *,uint32_t rng[3],const uint16_t *counts,size_t,
    const SmN64WebActionHost *,SmN64WebAbilityEvent *);
/* Optional admission at source early trap-modifier command boundaries, before
 * resources or actor callbacks. A declined modifier leaves the current trap
 * action running; NULL admission preserves the original API exactly. */
int smn64_web_action_step_admitted(SmN64WebAction *,const SmN64WebButtons *,uint32_t,
    SmN64WebResource *,uint32_t[3],const uint16_t *,size_t,
    const SmN64WebActionHost *,SmN64WebAbilityEvent *,SmN64CombatAdmission,void *);
#endif
