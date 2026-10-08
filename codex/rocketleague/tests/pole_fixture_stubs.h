#ifndef ROCKET_POLE_FIXTURE_STUBS_H
#define ROCKET_POLE_FIXTURE_STUBS_H
#include "../../../src/game/rocket_pole.h"
/* Existing component fixtures have no supported pyramid pole. */
#ifndef ROCKET_POLE_REAL_TEST
int rocket_pole_can_grab(struct MarioState*m,struct Object*o){(void)m;(void)o;return 0;}
void rocket_pole_prepare(struct MarioState*m){(void)m;}
int rocket_pole_execute(struct MarioState*m){(void)m;return -1;}
int rocket_pole_present(struct MarioState*m,RocketSnapshot*p){(void)m;(void)p;return 0;}
int rocket_pole_take_release(struct MarioState*m,RocketSnapshot*p,int*j){(void)m;(void)p;(void)j;return 0;}
void rocket_pole_forget(struct Object*o){(void)o;}
#endif
#endif
