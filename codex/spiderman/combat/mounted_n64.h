#ifndef SMN64_MOUNTED_H
#define SMN64_MOUNTED_H
#include "combo_owner_n64.h"

/* Continues the actual grab-owner transitions into source states0x4000000
 * (kick) and0x8000000 (mounted punch), after generic animation successors. */
typedef struct SmN64MountedOwner {
    SmN64ComboOwner player;
    int32_t velocity[3], up[3];
    uint32_t dismount_c9c, combo_metric, display_value, display_timer;
    uint8_t jump_latch_311, suit;
    int32_t difficulty;
} SmN64MountedOwner;
typedef struct SmN64MountedActor {
    uint32_t id;
    int16_t health;
} SmN64MountedActor;
typedef struct SmN64MountedHit {
    uint32_t actor, kind;
    uint16_t damage;
    uint8_t flags, has_knockback;
    int32_t direction[3];
    uint16_t impulse, duration;
} SmN64MountedHit;
typedef struct SmN64MountedHost {
    void *context;
    /* Lifetime lookup: 1 found, 0 expired, negative unavailable. */
    int (*actor)(void *, uint32_t handle, SmN64MountedActor *);
    /* Actual actor virtual damage call. 0 rejected and1 accepted both continue
     * source owner effects. Negative means unavailable/failure. Punch packets
     * specify ONLY actor/kind/damage/flags; direction/impulse/duration are valid
     * only with has_knockback. Never synthesize acceptance or generic damage. */
    int (*damage)(void *, SmN64MountedOwner *, const SmN64MountedHit *);
    /* Original9F9B8(player,position,0). Synchronous allocation/effects/shared RNG
     * boundary; return1 only once its actual work is committed. */
    int (*hit_effects)(void *, SmN64MountedOwner *, const int32_t position[3]);
    /* Original30920(sound,position,0), synchronous,1 success. */
    int (*sound)(void *, uint32_t sound, const int32_t position[3]);
    int (*stop)(void *, SmN64MountedOwner *); /* Original97DE4,1 success. */
} SmN64MountedHost;

/* Original8E1E8–8E4AC scalar routing and exact actor packets. Returns1handled,
 * 0other state,-1invalid,-2missing/failed host boundary. Input pressed bits are
 * shared with the same controller; mounted punch consumes punch every tick and
 * suppresses jump during clip125. The source ignores damage rejection and still
 * emits its corresponding effects. Missing/expired mounted-punch target would
 * dereference null in the original and is explicitly unsupported here.
 * Callback effects/RNG are committed in order. Any failure requires fail-stop
 * or rollback of the whole owner, not retrying a partially committed struct. */
int smn64_mounted_step(SmN64MountedOwner *, SmN64CombatInput *,
    uint16_t completed_animation, uint32_t tick, const uint16_t *counts, size_t,
    const SmN64MountedHost *);
#endif
