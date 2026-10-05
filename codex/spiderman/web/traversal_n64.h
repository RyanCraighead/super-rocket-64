#ifndef SMN64_WEB_TRAVERSAL_H
#define SMN64_WEB_TRAVERSAL_H
#include <stddef.h>
#include <stdint.h>
#include "../movement/n64_behavior.h"
#include "resource_n64.h"
/* USA 1.0 source-shaped fixed12/Y-down traversal. No host geometry assumed. */
typedef struct SmN64WebLine {
    int32_t distance;           /* original line40: whole native units */
    uint32_t hit, surface;      /* original line68/80: nonzero presence */
    int32_t position[3];        /* original line6c/70/74: fixed12 */
    int16_t normal[3];          /* original line78/7a/7c: fixed12 */
    uint16_t surface_flags;     /* original surface+0c */
} SmN64WebLine;
typedef enum SmN64WebQueryKind {
    SMN64_WEB_ZIP_B=1, SMN64_WEB_ZIP_R=2,
    SMN64_WEB_SWING_FIRST=3, SMN64_WEB_SWING_VERTICAL=4,
    SMN64_WEB_SWING_REFINE=5, SMN64_WEB_SWING_LATERAL=6,
    SMN64_WEB_SWING_EXIT=7, SMN64_WEB_CLEARANCE=8
} SmN64WebQueryKind;
/* Query order, endpoints and source args are retained by this module. Return1
 * supplies a line (hit may0); return<0 means unavailable/unsupported and is
 * propagated. Original collision backend query is (1,0,0,1). */
typedef int (*SmN64WebRay)(void *, SmN64WebQueryKind, const int32_t from[3],
                           const int32_t to[3], SmN64WebLine *);
typedef struct SmN64WebInput {
    uint8_t zip_held, swing_held, jump_held, jump_pressed;
    uint8_t web_pressed, punch_pressed, kick_pressed;
    int32_t kid_mode;            /* global800F5F1C */
    int32_t stable_updates;      /* global800F59B8, increments whenF59B4=0 */
} SmN64WebInput;
typedef struct SmN64WebPlayer {
    SmN64Anim anim;
    uint32_t state;             /*1118*/
    int32_t position[3], velocity[3];
    int32_t forward[3], right[3], outward[3]; /*F64,F70,F7C*/
    int16_t normal[3];          /*A8*/
    uint16_t body_offset;       /*11A0, normally96*/
    int32_t aiming, holding, lock; /*A2C,113C,66C*/
    int32_t adhered, wall_orientation, ceiling_orientation; /*CF0,A24,A28*/
    int32_t auxiliary_a38, turn_ticks, swing_scan_vertical, swing_scan_lateral;
    int32_t target[3], target_normal[3], zip_origin[3]; /*10B8,1098,688*/
    int32_t swing_target[3], anchor[3], second_anchor[3], second_web;
    int32_t airborne_owner, launch_flag, substate, kid_jump_gate; /*65C,660,111C,1184*/
    int32_t zip_graphic, swinger_present;
    uint8_t web_mode, aim_pressed, aim_copied; /*A44,input41,321*/
    uint16_t idle_ticks;
    uint32_t random_state[3];
    SmN64WebResource resource;
} SmN64WebPlayer;
/* Acquisition effects are emitted in original call order. Graphics are host
 * objects represented by source-equivalent lifetime flags, not N64 pointers. */
typedef enum SmN64WebEventKind {
    SMN64_WEB_DELETE_SWINGER=1, SMN64_WEB_CREATE_ZIP=2,
    SMN64_WEB_FIRE_ZIP=3, SMN64_WEB_CAMERA=4,
    SMN64_WEB_REFILL_SOUND=5, SMN64_WEB_EMPTY_VOICE=6,
    SMN64_WEB_RETRACT_ZIP=7, SMN64_WEB_DELETE_ZIP=SMN64_WEB_RETRACT_ZIP, SMN64_WEB_CREATE_SWINGER=8,
    SMN64_WEB_DETACH_STRAND=9, SMN64_WEB_SOUND_POSITION=10,
    SMN64_WEB_SOUND_GLOBAL=11, SMN64_WEB_CAMERA_TARGET=12,
    SMN64_WEB_CAMERA_TURN=13
} SmN64WebEventKind;
typedef struct SmN64WebEvent { SmN64WebEventKind kind; int32_t value, extra; } SmN64WebEvent;
typedef struct SmN64WebEvents { SmN64WebEvent events[16]; size_t count; } SmN64WebEvents;
struct SmN64Swinger;
/* Synchronous original graphic boundary. Constructors can consume the same
 * RNG stream as gameplay; delayed replay after voice RNG is NOT equivalent.
 * Callback acts on the enclosing frame's pending visual state and may advance
 * player.random_state. Exactly1 succeeds; any other result suspends/rolls back.
 * The parent owns rollback of its pending context alongside the copied player.
 * Only ordinary source-supported graphic effects may return success. */
typedef int (*SmN64WebLifecycle)(void *,SmN64WebEventKind,SmN64WebPlayer *,const struct SmN64Swinger *);

/* Eligibility mutates target/anchor fields exactly where original does, even
 * when later geometric checks reject a swing hit. -2: source division trap. */
int smn64_web_zip_eligible(const SmN64WebPlayer *,const SmN64WebLine *,int32_t max_distance);
int smn64_web_swing_prepare(SmN64WebPlayer *,const SmN64WebLine *);
/* Return1 acquired,0 rejected,negative unavailable/invalid. Count/callback
 * validation occurs before mutation. Supply original 300-clip frame counts.
 * Negative returns leave actor and events unchanged. Ordinary rejection0 may
 * retain original query counters and candidate fields.
 * Events count is appended, not reset: initialize it to0 per owner AI pass. */
int smn64_web_try_zip_b(SmN64WebPlayer *,const SmN64WebInput *,SmN64WebRay,void *,
                       const uint16_t *,size_t,SmN64WebEvents *);
int smn64_web_try_zip_r(SmN64WebPlayer *,const SmN64WebInput *,SmN64WebRay,void *,
                       const uint16_t *,size_t,SmN64WebEvents *);
int smn64_web_try_swing(SmN64WebPlayer *,const SmN64WebInput *,SmN64WebRay,void *,
                       const uint16_t *,size_t,SmN64WebEvents *);
/* Production acquisition variants require the synchronous graphics boundary.
 * The earlier three functions are bounded numeric/query kernels: allocation
 * effects are queued and their graphic RNG is deliberately outside that API. */
int smn64_web_try_zip_b_visual(SmN64WebPlayer *,const SmN64WebInput *,SmN64WebRay,void *,
                       const uint16_t *,size_t,SmN64WebEvents *,SmN64WebLifecycle);
int smn64_web_try_zip_r_visual(SmN64WebPlayer *,const SmN64WebInput *,SmN64WebRay,void *,
                       const uint16_t *,size_t,SmN64WebEvents *,SmN64WebLifecycle);
#endif
