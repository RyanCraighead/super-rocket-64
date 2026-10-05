#ifndef ROCKET_BOBOMB_H
#define ROCKET_BOBOMB_H
#include <PR/ultratypes.h>
struct Object;
/* Ordinary front-bumper policy only; caller owns the native consequence. */
int rocket_bobomb_bump_yaw(struct Object *bobomb,s16 *yaw);
void rocket_bobomb_forget(struct Object *bobomb);
#endif
