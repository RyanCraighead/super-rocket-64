#ifndef SM64_ROCKET_LAVA_H
#define SM64_ROCKET_LAVA_H
struct MarioState;
/* Contact only; the native floor handler still owns damage and the action. */
int rocket_lava_floor_contact(struct MarioState *m);
void rocket_lava_start(struct MarioState *m, unsigned actionArg);
/* Bounded horizontal controls during native lava bounce; never changes Y. */
int rocket_lava_update(struct MarioState *m);
#endif
