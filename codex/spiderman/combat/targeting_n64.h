#ifndef SMN64_TARGETING_H
#define SMN64_TARGETING_H
#include "combat_n64.h"
typedef struct SmN64TargetActor {
    uint32_t id,generation,flags_48,flags_3c8,field_3f0;
    uint16_t flags_0,enabled_dc,height_f4;
    int32_t position[3],cached_distance;
} SmN64TargetActor;
typedef struct SmN64TargetSlot { uint32_t actor,generation,age;int32_t direction[3]; } SmN64TargetSlot;
typedef struct SmN64Targeting {
    int32_t position[3];int16_t inverse[9];
    uint32_t aiming,target_actor,target_aux,marker_actor;
    int16_t look_angles[3];int32_t elapsed;
    /* Original camera matrixF5668 and integer positionF55F8/F55FC/F5600. */
    int16_t camera_matrix[9];int32_t camera_position[3];
    uint32_t awareness_tick,scan_tick,combo_metric,farthest,nearest,has_special,combo_active;
    SmN64TargetSlot slots[6];
} SmN64Targeting;
typedef struct SmN64TargetHost {
    void *context;
    int (*actor_at)(void *,size_t index,SmN64TargetActor *); /*0end,1actor,-1failure*/
    int (*lookup)(void *,uint32_t actor,uint32_t generation,SmN64TargetActor *); /*0expired,1found*/
    int (*line_clear)(void *,const int32_t from[3],const int32_t to[3]); /*AA364 flags0/0*/
    /* Synchronous9E3AC marker boundaries:1create and set marker_actor,2update
     * position plus yaw+=elapsed*16,3release/destroy. Constructor/script RNG,
     * if any, belongs to this callback in the shared stream. Exactly1 success. */
    int (*marker)(void *,uint32_t operation,SmN64Targeting *,const int32_t point[3]);
} SmN64TargetHost;
/* Exact4F870/4F73C/4F2B4/4F530 matrix slice: native input truncates to signed16,
 * low-word dot sums round through binary32 before /4096 and integer truncation. */
int smn64_target_transform(const int16_t matrix[9],const int32_t native[3],int32_t out[3]);
/* Pure A201C desired-heading math; then call original9F3D8/ClimbTurn with
 * fast0. This is a timed request, not the separate combo-init immediate turn. */
int smn64_target_face_heading(const SmN64Targeting *,const int32_t target[3],
                              uint16_t current_heading,uint8_t ceiling,uint16_t *desired);
/* Complete A1E18 ordered search, strict scores/ties and visibility-call gates.
 * Uses source cached actorE4, not a recomputed host-space distance. facing_weight0
 * skips local-facing transform AND minimum-facing test, as original combo/grab.
 * selected is cleared on no match. max_distance must be positive. */
int smn64_target_search(const SmN64Targeting *,int32_t max_distance,int32_t min_facing,
    int32_t distance_weight,int32_t facing_weight,const SmN64TargetHost *,SmN64TargetActor *selected);
/* Source9E3AC ordinary retained-target prepass at8D4BC: max2048,min2896,weights
 * 4096/4096. Aiming preserves10C4 while removing the ordinary marker. Do not
 * reselect inside air/FireWeb request handlers. */
int smn64_target_prepass(SmN64Targeting *,const SmN64TargetHost *);
/* Source9D6E4 existing six slots followed by9D850 scan, at8D4C4/8D4CC. Their
 * shared timers use original unsigned strict comparisons, including wrap. */
int smn64_target_awareness_update(SmN64Targeting *,uint32_t now,const SmN64TargetHost *);
int smn64_target_awareness_scan(SmN64Targeting *,uint32_t now,const SmN64TargetHost *);
/* Negative results are fail-stop or whole-owner rollback boundaries. Earlier
 * host callbacks, scalar target writes and any graphical RNG may have committed. */
#endif
