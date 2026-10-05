#ifndef SMN64_COMBAT_ENTRY_H
#define SMN64_COMBAT_ENTRY_H
#include "combat_n64.h"
typedef struct SmN64CombatEntry {
    SmN64Anim anim;
    uint32_t state,surface_mode,aiming,held_actor,target_actor;
    uint32_t nearby_actor,pickup_actor,interactable_actor,phase,entry_tick;
    uint32_t pickup_disabled;
    int32_t position[3],forward[3];
} SmN64CombatEntry;
typedef struct SmN64EntryActor {
    uint32_t id;
    uint16_t type,flags_4a,field_dc;
    uint32_t object_flags_10c;
    int32_t position[3],cached_distance;
} SmN64EntryActor;
typedef struct SmN64PickupTrace { uint32_t hit;int32_t distance; } SmN64PickupTrace;
typedef struct SmN64EntryHost {
    void *context;
    /* Enumerate in original list order. kind0=enemy listF5174,
     * kind1=pickup object listF6460; return0end,1actor,-1unavailable. */
    int (*actor_at)(void *,uint32_t kind,size_t index,SmN64EntryActor *);
    int (*actor_by_id)(void *,uint32_t id,SmN64EntryActor *);
    int (*line_clear)(void *,const int32_t from[3],const int32_t to[3]);
    int (*pickup_trace)(void *,const int32_t from[3],const int32_t to[3],SmN64PickupTrace *);
    int (*pickup_eligible)(void *,uint32_t id);
    /* Source800A0EB0(max256,minfacing2896,weights4096/4096). */
    int (*interactable)(void *,uint32_t *id);
} SmN64EntryHost;
typedef struct SmN64EntryEvent {
    uint32_t face_actor,immediate_face,heading;
    uint32_t began_combo,pickup_requested,throw_requested,interact_requested;
} SmN64EntryEvent;
/* Original A114C initializer target pass, also required on EVERY successor
 * combo. It uses cached actorE4 and original visibility, preserving actor order
 * and strict score ties. It resets only the three facing fields in event.
 * Commit immediate facing before source combo-start sound RNG/effects. The
 * interpreter's result.began tells the host when successor setup is required. */
int smn64_combat_combo_target(const int32_t position[3],const SmN64EntryHost *,SmN64EntryEvent *);
/* Complete scalar/world-query routing of99058, then exact authored combo init.
 * Facing is explicit. Original target-selection consumes cached actorE4; nearby
 * and pickup searches instead compute source native distance from positions. */
int smn64_combat_entry(SmN64CombatEntry *,SmN64Combo *,const SmN64CombatBank *,
    SmN64CombatInput *,uint32_t tick,const uint16_t *counts,size_t,
    const SmN64EntryHost *,SmN64EntryEvent *);
#endif
