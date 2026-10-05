#ifndef THPS1_SCORE_MOTION_H
#define THPS1_SCORE_MOTION_H
#include <stdint.h>
/* Original movement-scoring helper474c8 and main5a9b8 acceleration block.
 * Source input/output units are signedQ12 and nominal30Hz, respectively. */
int32_t thps1_score_grind_distance(const int32_t delta[3]);
void thps1_score_boost_step(int32_t *ticks,const int32_t velocity[3],int32_t acceleration[3]);
#endif
