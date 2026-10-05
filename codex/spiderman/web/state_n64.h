#ifndef SMN64_WEB_STATE_H
#define SMN64_WEB_STATE_H
#include "traversal_n64.h"
#include "swinger_n64.h"
#include "../climbing/climbing_n64.h"
typedef struct SmN64WebRuntime {
    SmN64WebPlayer player;
    SmN64Swinger swinger;
    SmN64ClimbBasis basis; /* owner synchronizes matrix/previous normal/yaw */
    uint32_t now, damage_tick, damage_flags; /* globalF965C,player610/614 */
    int32_t launch_velocity, base_timer, hold_timer;
    int32_t platform_present, platform_velocity_y;
    int32_t field_d20, field664, retained_forward[3];
    int32_t swing_length, swing_vertical_abs;
    int16_t swing_basis[9];
    int8_t analog_x, analog_y;
    int32_t run_ramp;
    uint16_t collision;
    SmN64WebLine side;
} SmN64WebRuntime;
typedef enum SmN64WebStateAction {
    SMN64_WEB_ACTION_JUMP=1, /* original9AA08 at completed zip clip272 */
    SMN64_WEB_ACTION_CEILING=2, /* original98CF8 during state200 */
    SMN64_WEB_ACTION_AIR_ATTACK=3, /* original9A7D8 before zip during state400 */
    SMN64_WEB_ACTION_STOP=4 /* original97DE4 from degenerate9D258 basis */
} SmN64WebStateAction;
/* Other source modules act on the supplied copy of actor state. Callbacks must
 * not cause external writes. Return1 performed,0 rejected,negative unavailable.
 * Their original effects need mapping by the owner; a missing callback is an
 * explicit failure whenever original dispatch reaches it. */
typedef int (*SmN64WebActionFn)(void *,SmN64WebStateAction,SmN64WebRuntime *,SmN64WebEvents *);
/* Marker and floor callbacks must return exactly1 and fill the output.
 * Zero and negative returns are unavailable and cause transactional rollback. */
typedef int (*SmN64WebMarker)(void *,const SmN64WebRuntime *,unsigned,int32_t out[3]);
typedef int (*SmN64WebFloor)(void *,const int32_t position[3],int32_t above,
                            int32_t below,int32_t include_objects,int32_t *result);
typedef struct SmN64WebServices {
    void *context;
    SmN64WebRay ray;
    SmN64WebMarker marker;
    SmN64WebFloor floor;
    SmN64WebActionFn action;
    SmN64WebLifecycle lifecycle;
} SmN64WebServices;
/* Exact numeric zip travel: source angle quantization and trig produce source
 * next-frame target velocity. Returns1, or-2 for the original DIV overflow
 * trap without mutation. It is not a normalized-vector lerp. */
int smn64_web_zip_velocity(const int32_t position[3],const int32_t target[3],int32_t velocity[3]);
int smn64_web_zip_reached(const int32_t position[3],const int32_t target[3],const int32_t normal[3]);
/* Eight original radial queries8009CF5C, applying each hit-normal correction
 * before constructing the next ray. distance is whole units, normally32. */
int smn64_web_clearance(SmN64WebPlayer *,int32_t distance,SmN64WebRay,void *);
/* Complete source state handlers40000,100,200,400 through source922C8.
 * Runtime/outputs commit only for success; missing world/action callbacks or a
 * degenerate basis return a negative code and must suspend, never fall back.
 * REQUIRED parent prepass: reset anim.rate=65536 (source8D280); clear collision
 * and side/ceiling hit markers before contact (source84200); copy the complete
 * contact.line into runtime.side after contact, including surface and flags.
 * External parent passes perform animation advancement, swinger_step(now),
 * source physics/contact owner, normal classification, input and jump tail.
 * This function does NOT advance physics or jump timers a second time.
 * Return1 handled; -1 invalid state/count, -3 query unavailable, -4 action/
 * marker/floor unavailable, -5 degenerate orientation dependency, -6 missing/
 * unsupported synchronous graphic lifecycle (including required shared RNG). */
int smn64_web_state_step(SmN64WebRuntime *,const SmN64WebInput *,const SmN64WebServices *,
                         const uint16_t *,size_t,SmN64WebEvents *);
#endif
