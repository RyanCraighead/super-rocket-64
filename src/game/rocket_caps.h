#ifndef ROCKET_CAPS_H
#define ROCKET_CAPS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct MarioState;
struct Object;
struct Packet;
/* Shared Wing/Metal/Vanish contract: one native special-cap mask and capTimer.
 * active_flags is gameplay state; visual_flags adds native warning flicker.
 * Neither API grants from remote appearance or changes the ordinary boost rule. */
uint32_t rocket_caps_active_flags(unsigned index);
uint32_t rocket_caps_visual_flags(unsigned index);
uint16_t rocket_caps_remaining(unsigned index);
void rocket_caps_before_mario_update(struct MarioState *m);
int rocket_caps_managed(const struct MarioState *m);
int rocket_caps_interact(struct MarioState *m, struct Object *cap);
void rocket_caps_update(void);
void rocket_caps_clear(unsigned index);
void rocket_caps_clear_all(void);
void rocket_caps_apply(struct MarioState *m);
int rocket_caps_packet_allowed(const struct Packet *p);
int rocket_caps_cancel_allowed(const struct Packet *p);
void rocket_caps_receive_cancel(struct Packet *p);
int rocket_caps_item_allowed(const struct Packet *p);
int rocket_caps_object_allowed(struct Packet *p);
void rocket_caps_receive(struct Packet *p);
#ifdef __cplusplus
}
#endif
#endif
