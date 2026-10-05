#ifndef ROCKET_WATER_MODE_H
#define ROCKET_WATER_MODE_H
#include <math.h>
/* Native body-origin thresholds. Hysteresis prevents surface jitter from
 * toggling the tank rule. No mode transition changes the finite balance. */
enum RocketWaterMode { ROCKET_WATER_DRY=0, ROCKET_WATER_JET=1, ROCKET_WATER_METAL=2 };
static inline int rocket_water_classify(int previous,int present,float level,float height,int metal) {
    if(!present||!isfinite(level)||!isfinite(height))return ROCKET_WATER_DRY;
    float depth=level-height;
    if(depth<=(previous==ROCKET_WATER_DRY?100.f:80.f))return ROCKET_WATER_DRY;
    return metal?ROCKET_WATER_METAL:ROCKET_WATER_JET;
}
#endif
