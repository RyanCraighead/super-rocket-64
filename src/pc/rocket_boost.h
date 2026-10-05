#ifndef ROCKET_BOOST_H
#define ROCKET_BOOST_H
#include <stdint.h>
#include "../../codex/rocketleague/physics/boost_mode.h"
#include "../../codex/rocketleague/physics/rocket_physics.h"
#include "../../codex/rocketleague/physics/difficulty_policy.h"
#define ROCKET_SESSION_RULE_BYTES 16
#ifdef __cplusplus
extern "C" {
#endif
struct Packet;
/* Effective session rule, never the client's saved preference. */
int rocket_boost_mode(void);
/* Join-bound authority token, shared by temporary effect packets. */
uint64_t rocket_boost_session_id(void);
int rocket_boost_can_set_mode(void);
int rocket_boost_set_mode(unsigned mode);
unsigned rocket_difficulty(void);
int rocket_difficulty_set(unsigned preset);
const char *rocket_difficulty_scope_label(void);
unsigned rocket_jump_percent(void);
int rocket_jump_set_percent(unsigned percent);
const char *rocket_jump_scope_label(void);
unsigned rocket_speed_percent(void);
float rocket_speed_scale(void);
/* Monotonic within an online session; offline changes include both independent preferences. */
uint32_t rocket_rule_revision(void);
int rocket_speed_set_percent(unsigned percent);
const char *rocket_speed_scope_label(void);
int rocket_surface_mode(void);
int rocket_surface_set_mode(unsigned mode);
const char *rocket_surface_scope_label(void);
const char *rocket_boost_mode_label(void);
const char *rocket_boost_scope_label(void);
void rocket_boost_session_reset(void);
void rocket_boost_network_update(void);
void rocket_boost_write_rule(struct Packet *packet);
int rocket_boost_join_valid(const struct Packet *packet);
void rocket_boost_read_join(struct Packet *packet);
int rocket_boost_packet_allowed(const struct Packet *packet);
void rocket_boost_receive_rule(struct Packet *packet);
#ifdef __cplusplus
}
#endif
#endif
