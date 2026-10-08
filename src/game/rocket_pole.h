#ifndef ROCKET_POLE_H
#define ROCKET_POLE_H
#include "../../codex/rocketleague/physics/rocket_physics.h"
struct MarioState;struct Object;
int rocket_pole_can_grab(struct MarioState *m,struct Object *pole);
void rocket_pole_prepare(struct MarioState *m);
/* Returns -1 outside the scoped local pole action; otherwise native cancel. */
int rocket_pole_execute(struct MarioState *m);
int rocket_pole_present(struct MarioState *m,RocketSnapshot *pose);
int rocket_pole_take_release(struct MarioState *m,RocketSnapshot *pose,int *jump);
void rocket_pole_forget(struct Object *object);
#endif
