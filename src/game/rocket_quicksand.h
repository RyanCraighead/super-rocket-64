#ifndef ROCKET_QUICKSAND_H
#define ROCKET_QUICKSAND_H
#include "../../codex/rocketleague/physics/rocket_physics.h"
struct MarioState;
/* Called before/after owned physics; native code owns type caps/death/hooks.
 * Returns true when a native action must take ownership immediately. */
int rocket_quicksand_update(struct MarioState *m,const RocketSnapshot *pose);
void rocket_quicksand_suspend(void);
/* Apply only to the actual merged local input, once per host logic frame. */
void rocket_quicksand_filter_input(RocketInput *input,int blocked);
float rocket_quicksand_depth(void);
#endif
