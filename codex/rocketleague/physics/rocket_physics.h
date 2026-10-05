#ifndef CODEX_ROCKET_PHYSICS_H
#define CODEX_ROCKET_PHYSICS_H
#include <stddef.h>
#include <stdint.h>
#include "water_mode.h"
#include "boost_mode.h"
#include "speed_policy.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Host adapter API, not original Psyonix code. RocketSim is a reconstruction.
 * Positions/linear velocities below use SM64 axes and units/second.
 * RL (x,y,z) -> SM64 (y,z,x)*2 is a proper rotation, not a reflection.
 * One host logic frame is exactly four 120 Hz physics steps. */
#define ROCKET_HOST_SCALE 2.0f
#define ROCKET_SUBSTEPS 4
#define ROCKET_TICK_SECONDS (1.0f/120.0f)
typedef struct RocketWorld RocketWorld;
/* Standalone worlds start at 100%; the game supplies its saved/host rule.
 * Valid changes preserve body, fuel, input gates and ability timers. */
int rocket_world_set_speed(RocketWorld *world, unsigned percent);
unsigned rocket_world_speed(RocketWorld *world);
/* Matches the settings sibling contract: snapshots always carry finite fuel.
 * Infinite steps borrow an allowance, preserving the stored balance. */
int rocket_world_set_boost_mode(RocketWorld *world, int mode);
int rocket_world_boost_mode(RocketWorld *world);
/* Native temporary cap allowance leaves the host preference and coin tank intact. */
void rocket_world_set_temporary_boost(RocketWorld *world,int active);
/* Only accepted local pickups may call this (one object = 5 points).
 * Consumption remains RocketSim's per-tick arithmetic, with recharge disabled. */
int rocket_world_collect_coin(RocketWorld *world);
enum { ROCKET_SURFACES_CAR = 0, ROCKET_SURFACES_NATIVE = 1 };
enum { ROCKET_MATERIAL_NORMAL = 0, ROCKET_MATERIAL_SLIPPERY = 1,
       ROCKET_MATERIAL_VERY_SLIPPERY = 2, ROCKET_MATERIAL_SLIDING = 4,
       /* Restricted host tag: CCM indoor native ice race, not general ice. */
       ROCKET_MATERIAL_RACE_SLIDE = 8 };
typedef struct RocketTriangle { float v[3][3]; uint8_t material; } RocketTriangle;
typedef struct RocketEnvironment {
    /* Host axes; temporary current/wind displacement velocity (units/second),
     * plus true velocity acceleration (units/second squared) for updrafts. */
    float drift[3], acceleration[3];
} RocketEnvironment;
/* A complete per-host-frame moving-platform snapshot. Triangle vertices use
 * platform-local SM64 axes/units; basis stores host-space XYZ columns. */
typedef struct RocketPlatform {
    uint64_t object_id;
    float position[3], basis[9];
    const RocketTriangle *triangles;
    size_t count;
} RocketPlatform;
typedef struct RocketInput {
    float throttle, steer, pitch, yaw, roll;
    int jump, boost, powerslide;
} RocketInput;
typedef struct RocketSnapshot {
    float position[3], velocity[3];
    /* Local forward/right/up columns, transformed into host axes. */
    float basis[9], angular_velocity[3];
    float wheel_position[4][3], wheel_steer[4], wheel_radius[4];
    float boost, jump_time, flip_time, air_time;
    uint64_t ticks;
    int grounded, wheel_contacts[4], jumped, double_jumped, flipped, flipping;
    /* Actual backend thrust, including finite-fuel exhaustion, not held input. */
    int boosting;
    /* Derived from native environment/action, never added to the pose wire ABI. */
    int water_mode;
} RocketSnapshot;
RocketWorld *rocket_world_create(void);
void rocket_world_destroy(RocketWorld *world);
/* Render/event paths may observe a pause even when gameplay is not ticking. */
void rocket_world_interrupt(RocketWorld *world);
/* Native water geometry and cap state, refreshed by the owning adapter. */
void rocket_world_set_water(RocketWorld *world,int present,float level,int metal);
void rocket_world_set_water_current(RocketWorld *world,const float velocity[3]);
/* Optional native geometry query at each physics substep, including water-box
 * edges. Returns presence and writes the current surface height in host units. */
typedef int (*RocketWaterQuery)(float x,float z,float *level);
void rocket_world_set_water_query(RocketWorld *world,RocketWaterQuery query);
int rocket_world_set_environment(RocketWorld *world, const RocketEnvironment *environment);
void rocket_world_set_surface_mode(RocketWorld *world, unsigned mode);
/* Local native Metal Cap underwater mode, never received from a pose packet. */
void rocket_world_set_metal_water(RocketWorld *world, int active);
/* Complete replacement of one static collision layer. */
int rocket_world_mesh(RocketWorld *world, int dynamic_layer,
                      const RocketTriangle *triangles, size_t count);
/* Complete object-keyed kinematic platform snapshot, before each simulated
 * frame. Omitted IDs are removed. */
int rocket_world_platforms(RocketWorld *world, const RocketPlatform *platforms,
                           size_t count);
int rocket_world_reset(RocketWorld *world, const float position[3],
                       const float velocity[3], float yaw_radians);
/* Expiry recovery: reposition only; preserve consumed boost and ability timers.
 * Stops motion and clears contact history. Never grants cap permissions. */
int rocket_world_recover(RocketWorld *world, const RocketSnapshot *clear_pose);
/* Paused frames freeze physics/timers; blocked input becomes neutral while
 * gravity/contact continue. Buttons held through either require release.
 * Duplicate and older frame IDs never simulate or queue catch-up ticks.
 * Frame IDs are local monotonic uint64 values; reset begins a new epoch. */
int rocket_world_frame(RocketWorld *world, uint64_t frame_id,
                       const RocketInput *input, int paused, int input_blocked);
/* Only the session-rule adapter calls this. Switching never awards finite boost.
 * Infinite steps borrow a full tank and restore the finite balance afterward. */
int rocket_world_set_boost_mode(RocketWorld *world, int mode);
/* Bounded owner-local velocity change; no reset, fuel or ability modification. */
int rocket_world_bump(RocketWorld *world,const float delta_velocity[3]);
int rocket_world_snapshot(RocketWorld *world, RocketSnapshot *out);
const char *rocket_world_error(void);
void rocket_to_host(const float rl[3], float host[3], float scale);
void rocket_from_host(const float host[3], float rl[3], float scale);
#ifdef __cplusplus
}
#endif
#endif
