#ifndef PLAYER_BUMP_H
#define PLAYER_BUMP_H
#include <stdbool.h>
struct Packet;
struct MarioState;
struct CharacterNetState;
void player_bump_observe(unsigned index,const struct CharacterNetState *state,const float position[3],const float velocity[3]);
void player_bump_update(void);
void player_bump_clear(unsigned index);
bool player_bump_packet_allowed(struct Packet *packet);
void player_bump_receive(struct Packet *packet);
/* Prevent native torso push/stomp/PvP from duplicating a car contact. */
int player_bump_car_pair(const struct MarioState *a,const struct MarioState *b);
#endif
