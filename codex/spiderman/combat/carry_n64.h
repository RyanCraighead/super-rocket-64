#ifndef SMN64_CARRY_H
#define SMN64_CARRY_H
#include "combat_n64.h"
typedef struct SmN64Carry {
    SmN64Anim anim;
    uint32_t state,held_actor,pickup_actor,target_actor;
    int32_t position[3],forward[3],up[3];
} SmN64Carry;
typedef struct SmN64CarryObject {
    uint32_t id,flags_10c;
    int32_t position[3];
    int16_t hold_radius,yaw;
} SmN64CarryObject;
typedef struct SmN64ThrowRequest {
    uint32_t actor,use_path,path_ticks;
    int32_t velocity[3],target[3];
    uint16_t spin;
} SmN64ThrowRequest;
typedef struct SmN64CarryHost {
    void *context;
    int (*object)(void *,uint32_t id,SmN64CarryObject *);
    int (*pickup)(void *,uint32_t id); /*original7F760 flags/alpha/script event*/
    int (*throw_object)(void *,const SmN64ThrowRequest *);
    int (*stop)(void *,SmN64Carry *); /*original97DE4 held-object stop clips*/
    int (*jump)(void *,SmN64Carry *);
} SmN64CarryHost;
typedef struct SmN64CarryEvent { uint32_t camera_reset,sound; } SmN64CarryEvent;
/* Ordered native release boundary: owns the one Rnd32 draw after the source
 * alpha/field writes (or after path allocation). request.spin is0 on entry.
 * Uses SmN64CarryHost.context. Callback failure is fail-stop, not retryable. */
typedef int (*SmN64CarryOrderedThrow)(void *,const SmN64ThrowRequest *,uint32_t rng[3]);
int smn64_carry_step_ordered(SmN64Carry *,uint32_t rng[3],const SmN64CarryHost *,
    SmN64CarryEvent *,SmN64CarryOrderedThrow);

int smn64_carry_step(SmN64Carry *,uint32_t rng[3],const SmN64CarryHost *,SmN64CarryEvent *);
/* Original939CC holding point from authored markers1 and0. Integer division
 * by2 truncates towardzero, distinct from the arithmetic midpoint in FireWeb. */
int smn64_carry_position(const int32_t marker1[3],const int32_t marker0[3],
    const int32_t player[3],const int32_t forward[3],int16_t radius,
    uint16_t previous_yaw,uint16_t yaw_delta,int32_t out[3],uint16_t *yaw);
/* Original7E5E0 far throw curve. Caller supplies capacity at leastticks.
 * Normal source routing usesdistance/32 withdistance>=1024, thus ticks>=32.
 * Returns1success,-1invalid/undefined original division inputs; transactional. */
int smn64_throw_path(const int32_t object[3],const int32_t target[3],uint32_t ticks,
    int32_t (*points)[3],size_t capacity);
#endif
