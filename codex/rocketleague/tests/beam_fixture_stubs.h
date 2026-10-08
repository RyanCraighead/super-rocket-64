#ifndef ROCKET_BEAM_FIXTURE_STUBS_H
#define ROCKET_BEAM_FIXTURE_STUBS_H
#include "../../../src/game/rocket_beam.h"
#ifndef ROCKET_BEAM_REAL_TEST
/* This boundary is exercised with the actual implementation in test_beam. */
void rocket_beam_reset(void){}
void rocket_beam_update(struct MarioState *m,const RocketSnapshot *pose){(void)m;(void)pose;}
#endif
#endif
