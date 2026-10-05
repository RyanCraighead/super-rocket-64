#ifndef BOSS_NET_H
#define BOSS_NET_H
#include <stdint.h>
struct Object;
struct Packet;
int boss_net_enabled(void);
int boss_net_managed(const struct Object *object);
int boss_net_simulates(const struct Object *object);
uint32_t boss_net_epoch(const struct Object *object);
/* Begin may perform a voluntary, revisioned handoff before any native action. */
int boss_net_begin(struct Object *object);
void boss_net_end(struct Object *object);
void boss_net_update(void);
void boss_net_reset(void);
void boss_net_disconnected(unsigned global);
int boss_net_object_packet(struct Object *object,struct Packet *packet);
int boss_net_wrap_effect(struct Packet *packet);
void boss_net_receive(struct Packet *packet);
int boss_net_legacy_packet_valid(struct Packet *packet);
int boss_net_applying(void);
int boss_net_player_holds(struct Object *object,unsigned global);
int boss_net_reward_available(struct Object *object);
#endif
