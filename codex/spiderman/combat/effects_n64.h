#ifndef SMN64_COMBAT_EFFECTS_H
#define SMN64_COMBAT_EFFECTS_H
#include <stdint.h>
/* Synchronous original shared-RNG effects. Run at the original event boundary,
 * not later in rendering. Reuse these generated values to draw; don't reroll. */
typedef struct SmN64HitParticle { int32_t velocity[3];uint16_t life; } SmN64HitParticle;
typedef struct SmN64SpecialHitParticle { int32_t velocity[3];uint16_t size;int16_t spin; } SmN64SpecialHitParticle;
typedef struct SmN64HitEffects {
    uint32_t sound,style,particle_count,special_count;
    uint16_t ring_rotation[2];
    SmN64HitParticle particles[12];
    SmN64SpecialHitParticle special[10];
} SmN64HitEffects;
/* Original A114C always RNG(4)+10, including combo successors. */
uint32_t smn64_combo_start_sound(uint32_t rng[3]);
/* Original accepted-hit audio then9F9B8 effects, including two64E00 rings.
 * web_type and glove_hits are AFTER the hit's source glove decrement. */
int smn64_combat_hit_effects(uint16_t animation,uint32_t web_type,
    uint32_t glove_hits,uint32_t rng[3],SmN64HitEffects *);
/* Audio-free original9F9B8 effect boundary. For mounted punch use style0 and
 * current post-successor clip (often128), then emit fixed sound16 AFTER them. The
 * bundled melee helper above would incorrectly add an audio RNG draw there. */
int smn64_combat_hit_particles(uint32_t style,uint16_t animation,uint32_t web_type,
    uint32_t glove_hits,uint32_t rng[3],SmN64HitEffects *);
typedef struct SmN64ImpactDebris {
    int32_t endpoint_offset[2][3];
    uint8_t size_class;
} SmN64ImpactDebris;
/* Original actor-impact fragment loop B273C: at most30. count is the number
 * allowed by the source effect-pool availability checks; each entry consumes
 * sixRNG(21) and then the B2884(mode1) constructor's twoRNG(3). */
int smn64_impact_debris(uint32_t count,uint32_t rng[3],SmN64ImpactDebris out[30]);
#endif
