#ifndef SMN64_STRAND_H
#define SMN64_STRAND_H
#include <stdint.h>
/* Original ordinary(type0) procedural web strand, not a guessed straight line.
 * All position vectors are signed fixed12. Camera is whole native units as in
 * source global800F55F8/55FC/5600. Point count remains fixed after creation. */
typedef struct SmN64StrandPoint {
    int32_t base[3];
    uint8_t amplitude, rate;
    uint16_t phase;
    int8_t offset;
    uint8_t interpolation;
} SmN64StrandPoint;
typedef struct SmN64Strand {
    int32_t count;
    int32_t anchor[3], perpendicular[3];
    int32_t primary[40][3], secondary[80][3], particles[40][3];
    SmN64StrandPoint point[40];
    uint8_t jitter, wobble, bend_trigger, bend, bend_decay;
} SmN64Strand;
/* Source800B4108/800B15AC: clamp(integer distance/80,1,40). */
int32_t smn64_strand_point_count(const int32_t from[3],const int32_t to[3]);
/* Exactly four source RNG calls per point:100,4096,192,19, in that order.
 * Calls this at the source constructor point BEFORE subsequent voice RNG.
 * Initializes geometry by original6B78C/A9950/B4068 arithmetic. */
int smn64_strand_init(SmN64Strand *,const int32_t from[3],const int32_t to[3],
                      const int32_t camera[3],uint32_t random_state[3]);
/* OriginalB4068: resets the base points each time endpoints move and recomputes
 * camera-facing perpendicular. It does not choose a new count or new RNG. */
int smn64_strand_endpoints(SmN64Strand *,const int32_t from[3],const int32_t to[3],
                           const int32_t camera[3]);
/* Original B190C ordinary(type0) geometry path: wobble, damped bend, zigzag
 * secondary chain and source particle positions. Jitter78 optionally consumes
 * two RNG calls per point. Fire-web color/spark branch80!=0 is NOT included. */
int smn64_strand_step(SmN64Strand *,uint32_t absolute_tick,uint32_t random_state[3]);
/* Original material contract: both line chains RGB=(162,162,162). N64 list
 * emits G_LINE3D with width field0. Per-point sprite effect-slot4 resolves to
 * texture slot46 in this revision, source scale0.4, source54=128/source56=90.
 * Texture bytes, GPU state, visibility/LOD and sprite projection are not bundled. */
#endif
