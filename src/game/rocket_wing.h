#ifndef ROCKET_WING_H
#define ROCKET_WING_H
#include "rocket_caps.h"
struct MarioState;
struct Object;
struct Packet;
/* The native cap timer/status is the only offline source. Online grants are
 * issued by the host after contact with an existing, tangible Wing Cap. */
int rocket_wing_active(unsigned index);
/* Local gameplay/HUD override; the saved/session ordinary mode is untouched. */
int rocket_wing_boost_mode(void);
void rocket_wing_topper_update(void);
void rocket_wing_topper_clear(unsigned index);
#endif
