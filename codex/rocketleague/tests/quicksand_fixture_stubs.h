#ifndef ROCKET_QUICKSAND_FIXTURE_STUBS_H
#define ROCKET_QUICKSAND_FIXTURE_STUBS_H
#include "../../../src/game/rocket_quicksand.h"
/* Existing component fixtures have no sand. The new bridge/adapter suite
 * links the real native sand code instead. Never linked into the game. */
#ifndef ROCKET_QUICKSAND_REAL_TEST
int rocket_quicksand_update(struct MarioState*m,const RocketSnapshot*p){(void)m;(void)p;return 0;}
void rocket_quicksand_suspend(void){}
void rocket_quicksand_filter_input(RocketInput*i,int blocked){(void)i;(void)blocked;}
float rocket_quicksand_depth(void){return 0;}
#endif
#endif
