#ifndef ROCKET_NATIVE_WATER_H
#define ROCKET_NATIVE_WATER_H
#include "sm64.h"
#include "../../codex/rocketleague/physics/water_mode.h"
/* PACKET_PLAYER carries action/flags and the car pose atomically. Derive remote
 * presentation from that accepted owner state, never a new client power bit. */
static inline int rocket_water_native_mode(u32 action,u32 flags) {
    switch(action) {
        case ACT_WATER_IDLE:
            return (flags&MARIO_METAL_CAP)?ROCKET_WATER_METAL:ROCKET_WATER_JET;
        case ACT_METAL_WATER_STANDING:case ACT_METAL_WATER_WALKING:
        case ACT_METAL_WATER_FALLING:case ACT_METAL_WATER_FALL_LAND:
        case ACT_METAL_WATER_JUMP:case ACT_METAL_WATER_JUMP_LAND:
            // Native cap expiry runs after the adapter. A final metal-water
            // action with the flag cleared is already ordinary submerged mode.
            return (flags&MARIO_METAL_CAP)?ROCKET_WATER_METAL:ROCKET_WATER_JET;
        default:return ROCKET_WATER_DRY;
    }
}
#endif
