#ifndef SMN64_AIR_INTERRUPT_N64_H
#define SMN64_AIR_INTERRUPT_N64_H
#include "air_attack_n64.h"
enum SmN64AirInterruptPhase {
    SMN64_AIR_INTERRUPT_START_TRAILS=1,
    SMN64_AIR_INTERRUPT_ALIGN_FLAT=2,
    SMN64_AIR_INTERRUPT_STOP_TRAILS=3
};
typedef struct SmN64AirInterruptHost {
    void *context;
    /* Synchronous at the original call position. ALIGN_FLAT is specifically
     * 9D258(force=0, forward=NULL), observing OLD state/clip/velocity. Reflect
     * any source basis/fallback changes back into s before returning exactly1.
     * START/STOP are genuine A22C0/A2290 graphical lifecycle operations. */
    int (*phase)(void *, SmN64AirAttack *s, uint32_t phase);
} SmN64AirInterruptHost;
/* Source 916A0 prelude + recent-damage branch91704..9177C/91A34.
 * 1=interrupted into state4;0=continue ordinary air tick;-1=bad input/count;
 * -2=required host operation failed. Existing state structs/ABI are unchanged.
 * motion_speed119E is zeroed for every valid state1000000 invocation, even
 * when no interruption is selected. Other aliases change only as documented.
 * prior_damage_state614 is the state retained by accepted damage, not packet
 * flags. delta(now,damage_tick610) uses wrapped unsigned arithmetic. */
int smn64_air_attack_interrupt(SmN64AirAttack *,uint32_t prior_damage_state,
    uint32_t damage_tick,uint32_t now,uint32_t first_trail_present,
    uint32_t *jump_variant_1180,uint16_t *motion_speed_119e,
    const uint16_t *counts,size_t,const SmN64AirInterruptHost *);
#endif
