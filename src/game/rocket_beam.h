#ifndef ROCKET_BEAM_H
#define ROCKET_BEAM_H
#include "../../codex/rocketleague/physics/rocket_physics.h"
struct MarioState;
void rocket_beam_reset(void);
void rocket_beam_update(struct MarioState *m,const RocketSnapshot *pose);
#endif
