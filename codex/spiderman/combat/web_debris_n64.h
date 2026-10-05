#ifndef SMN64_WEB_DEBRIS_H
#define SMN64_WEB_DEBRIS_H
#include <stdint.h>
/* Original B2884/B2AC8 two connected web-line fragment, represented by three
 * vertices (prior, shared, next). Coordinates/velocities are native fixed12. */
typedef struct SmN64WebDebris {
    int32_t position[3][3],velocity[3][3],floor_y;
    uint8_t shade,fade_step,alive;
} SmN64WebDebris;
/* source mode0 sheet release: three RNG21 jitter draws then two RNG3 size
 * draws. mode1 impact fragment: only two RNG3 size draws; caller has already
 * sampled its impact endpoint offsets. Do NOT also call the old debris-RNG
 * helpers for the same fragment. No elapsed-time scaling in the original tick. */
int smn64_web_debris_init(SmN64WebDebris *,const int32_t vertices[3][3],
    const int32_t center[3],int32_t floor_y,int32_t speed,uint8_t mode,uint32_t rng[3]);
int smn64_web_debris_tick(SmN64WebDebris *);
#endif
