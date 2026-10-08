#ifndef ROCKET_PENGUIN_FIXTURE_STUBS_H
#define ROCKET_PENGUIN_FIXTURE_STUBS_H
#include "../../../src/game/rocket_penguin.h"
/* Older component fixtures have no penguins. The carry suite links real code. */
#ifndef ROCKET_PENGUIN_REAL_TEST
int rocket_penguin_carried(const struct MarioState *m){(void)m;return 0;}
void rocket_penguin_prepare(struct MarioState*m,const RocketSnapshot*p,const RocketInput*i){(void)m;(void)p;(void)i;}
void rocket_penguin_update(struct MarioState*m,const RocketSnapshot*p){(void)m;(void)p;}
void rocket_penguin_suspend(struct MarioState*m){(void)m;}
void rocket_penguin_forget(struct Object*o){(void)o;}
int rocket_penguin_drop_position(struct MarioState*m,struct Object*o,float p[3]){(void)m;(void)o;(void)p;return 0;}
void rocket_penguin_render_held(struct Object*o){(void)o;}
void rocket_penguin_filter_input(RocketInput*i){(void)i;}
int rocket_penguin_hint(void){return 0;}
#endif
#endif
