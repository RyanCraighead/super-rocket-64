#ifndef ROCKET_QUICKSAND_POLICY_H
#define ROCKET_QUICKSAND_POLICY_H
#include <math.h>
/* Native update_walking_speed target factor. Only wheel propulsion uses it;
 * airborne gravity, boost and the configured car-speed rule stay independent. */
static inline float rocket_quicksand_mobility(float depth){return isfinite(depth)&&depth>10.f?6.25f/fminf(depth,200.f):1.f;}
#endif
