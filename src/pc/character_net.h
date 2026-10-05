#ifndef CHARACTER_NET_H
#define CHARACTER_NET_H
#include "character_net_codec.h"
struct MarioState;
struct Packet;
int character_net_snapshot(unsigned index,RocketSnapshot *out);
int character_net_player_fresh(unsigned index);
/* -1 stale/ineligible; 0 native Mario; 1 driving car with latest accepted pose.
 * This never samples/interpolates a render pose for gameplay validation. */
int character_net_pickup_pose(unsigned index,RocketSnapshot *out);
void character_net_clear(unsigned index);
void character_net_clear_all(void);
/* Committed wheel selection when enabled; CLI remains the launch fallback. */
unsigned character_net_local_kind(void);
int character_net_write(struct Packet *packet);
int character_net_read(struct Packet *packet,unsigned index,CharacterNetState *state);
int character_net_accept(unsigned index,const CharacterNetState *state);
/* Fresh raw accepted pose for authoritative contacts; never interpolated. */
int character_net_interaction_snapshot(unsigned index,CharacterNetState *state);
int character_net_interaction_state(unsigned index,CharacterNetState *state,uint32_t *generation);
int character_net_contact(unsigned index,CharacterNetState *state,unsigned *generation);
int character_net_is_car(unsigned index);
/* Physical tire weight remains valid during input capture; never accepts presentation. */
int character_net_support_state(unsigned index,CharacterNetState *state,uint32_t *generation);
int character_net_remote_update(struct MarioState *m);
/* Fresh accepted owner mode; never a local gameplay authority. */
int character_net_water_mode(unsigned index);
void character_net_draw(const float view[16],const float projection[16],const int viewport[4]);
#ifdef ROCKET_CAR_QA
int character_net_qa_hidden(void);
void character_net_qa_observe(void *window);
#endif
#endif
