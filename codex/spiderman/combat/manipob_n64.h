#ifndef SMN64_MANIPOB_H
#define SMN64_MANIPOB_H
#include "throwable_n64.h"

/* Represented fields of original N64 CManipOb (not the PC layout).
 * Caller owns model/list/allocator resources. A new source allocation is zeroed
 * before its constructor. Positions, angles and authored debris are separate
 * constructor inputs; this module never invents an authored model or list. */
typedef struct SmN64ManipOb {
    SmN64Throwable motion;
    uint16_t flags, type, node, sound;
    int16_t hold_radius;
    uint16_t body_flags_4a;
    uint32_t shadow_id, pulse_sent, debris_count;
} SmN64ManipOb;

typedef struct SmN64ManipObHost {
    void *context;
    /* 590AC: set the matching dynamic-model registry entry alpha, if present. */
    int (*alpha)(void *,uint32_t owner,uint8_t alpha);
    /* 59394: look up this authored environment identifier and set its flags. */
    int (*hide_shadow)(void *,uint32_t shadow_id);
    /* A8144/A83F0: resolve this node's links and pulse each in source order.
     * An explicitly empty link list is valid. Missing data is not empty. */
    int (*pulse)(void *,uint16_t node);
    /* 30920(sound,position,0). Audio output can be a separately declared host
     * boundary, but this event must be retained rather than discarded. */
    int (*sound)(void *,uint32_t sound,const int32_t position[3]);
    /* AA280(position,0,700,0,1). Return1 after a real query; result -1 means
     * no ground. N64 Y increases downward. */
    int (*ground)(void *,const int32_t position[3],int32_t above,
                  int32_t below,uint32_t environment,int32_t *ground_y);
    /* 61744: nearby-baddy AI stimulus, NOT a visual ground effect. An explicit
     * registry with no eligible flags4A&0x200 recipients is a valid empty result.
     * Other hosts must implement the source recipient behavior or fail. */
    int (*stimulus)(void *,const int32_t position[3],uint32_t radius);
    int (*destroy)(void *,uint32_t owner);
} SmN64ManipObHost;

enum SmN64ManipObRelease {
    SMN64_MANIPOB_THROW = 0,
    SMN64_MANIPOB_DROP = 1,
    SMN64_MANIPOB_THROW_PATH = 2
};

/* Constructor-owned scalar writes at81BB0 and7E43C. Does not zero memory,
 * initialize model resources, parse positions/angles or apply the stage2 model
 * 44EF15C1 position adjustment. Call on a zeroed allocation for a new object. */
int smn64_manipob_construct_scalars(SmN64ManipOb *,uint16_t node,
    uint16_t authored_flags,int16_t hold_radius,uint16_t sound,uint32_t shadow_id);
/* 7F760. Flags OR820, alpha128, optional shadow, then once-only pulse. */
int smn64_manipob_pickup(SmN64ManipOb *,const SmN64ManipObHost *);
/* 7F698/7F614 or the post-path-construction tail of7F714. The full variant
 * consumes one shared RNG draw AFTER alpha and the earlier field writes.
 * PATH assumes caller already committed source path allocation/count/index. */
int smn64_manipob_release(SmN64ManipOb *,enum SmN64ManipObRelease,
    const int32_t velocity[3],uint32_t rng[3],const SmN64ManipObHost *);
/* Adapter for carry_n64's existing request boundary, which already consumed
 * Rnd32. spin is exactly that request's64+Rnd32; never draw again. */
int smn64_manipob_release_with_spin(SmN64ManipOb *,enum SmN64ManipObRelease,
    const int32_t velocity[3],uint16_t spin,const SmN64ManipObHost *);
/* 7E880 ordinary EMPTY authored debris list path. Explosive bit20 or a nonempty
 * list returns-3 before any host effect. Normal is unused on this source path.
 * Returns1, -1 invalid/missing dependency, -2 callback failure. Callback errors
 * are fail-stop: earlier state/effects may have committed; never local retry. */
int smn64_manipob_impact_empty(SmN64ManipOb *,const SmN64ManipObHost *);
/* 7E7D8 Smash: overwrite velocity, impact, optional shadow, once pulse, destroy.
 * Only the same ordinary empty-debris profile is supported. */
int smn64_manipob_smash_empty(SmN64ManipOb *,const SmN64ManipObHost *);
#endif
