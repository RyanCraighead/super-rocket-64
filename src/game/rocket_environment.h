#ifndef ROCKET_ENVIRONMENT_H
#define ROCKET_ENVIRONMENT_H
#include "pc/rocket_runtime.h"
struct MarioState;
struct Surface;
/* Native source semantics sampled without mutating Mario's movement state. */
void rocket_environment_sample(struct MarioState *m, const RocketSnapshot *pose, RocketEnvironment *out);
unsigned rocket_environment_material(struct MarioState *m, struct Surface *surface);
#endif
