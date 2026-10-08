#ifndef ROCKET_PENGUIN_H
#define ROCKET_PENGUIN_H
#include "pc/rocket_runtime.h"
struct MarioState;
struct Object;
/* Native held-object identity; no additional actor or network payload. */
int rocket_penguin_carried(const struct MarioState *m);
void rocket_penguin_prepare(struct MarioState *m,const RocketSnapshot *pose,const RocketInput *input);
void rocket_penguin_update(struct MarioState *m,const RocketSnapshot *pose);
void rocket_penguin_suspend(struct MarioState *m);
void rocket_penguin_forget(struct Object *object);
int rocket_penguin_drop_position(struct MarioState *m,struct Object *object,float position[3]);
void rocket_penguin_render_held(struct Object *object);
void rocket_penguin_filter_input(RocketInput *input);
/* 0 hidden, 1 parked beside a pickup, 2 parked carrying, 3 moving carrying. */
int rocket_penguin_hint(void);
#endif
