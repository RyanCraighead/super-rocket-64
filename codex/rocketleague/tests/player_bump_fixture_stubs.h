#ifndef PLAYER_BUMP_FIXTURE_STUBS_H
#define PLAYER_BUMP_FIXTURE_STUBS_H
#ifndef PLAYER_BUMP_REAL_TEST
#include "pc/player_bump.h"
void player_bump_clear(unsigned i){(void)i;}
void player_bump_observe(unsigned i,const struct CharacterNetState *s,const float p[3],const float v[3]){(void)i;(void)s;(void)p;(void)v;}
bool player_bump_packet_allowed(struct Packet *p){(void)p;return false;}
void player_bump_receive(struct Packet *p){(void)p;}
int player_bump_car_pair(const struct MarioState *a,const struct MarioState *b){(void)a;(void)b;return 0;}
#endif

#endif
