#ifndef SMN64_COMBO_OWNER_H
#define SMN64_COMBO_OWNER_H
#include "combat_n64.h"
#include "web_abilities_n64.h"

/* Scalar fields shared with the player owner. Actor IDs represent lifetime-
 * checked host handles, not unchecked native addresses. Keep anim/combo_id in
 * sync with the interpreter and shared web fields in sync with web_action_n64. */
typedef struct SmN64ComboOwner {
    SmN64Anim anim;
    uint32_t state, entry_tick, grab_actor, grab_tick, target_actor;
    uint32_t heading_658, fired, aim_snapshot, yank_variant, look_active;
    uint16_t combo_id, motion_speed;
    uint8_t attack_mode, actor_flags;
    int8_t axis_1123, axis_1124;
    int32_t position[3], forward[3];
} SmN64ComboOwner;

typedef struct SmN64GrabActor {
    uint32_t id;
    uint16_t type;
    int32_t position[3];
} SmN64GrabActor;

typedef struct SmN64ComboOwnerHost {
    void *context;
    /* Original 9AC50 and 9AA08 respectively: 0 continue, 1 transitioned,
     * negative unavailable. These callbacks own all associated player changes. */
    int (*lost_ground)(void *, SmN64ComboOwner *);
    int (*jump)(void *, SmN64ComboOwner *);
    /* Full synchronous 9FCBC boundary, not just scalar combo_tick: root queries,
     * contacts and successor facing/initialization/start RNG must finish here.
     * Set source result ID in status. Return exactly 1 on success. */
    int (*interpret)(void *, SmN64ComboOwner *, SmN64CombatInput *, uint32_t tick,
                     int32_t *status);
    /* A1E18 query, then create a lifetime-checked handle (including a null one).
     * Return 1 on success; no match is successful with *actor == 0. */
    int (*select_target)(void *, SmN64ComboOwner *, int32_t max_distance,
                         int32_t min_facing, int32_t distance_weight,
                         int32_t facing_weight, uint32_t *actor);
    /* Original A201C for a nonnull actor; must finish before actor_flags clears.
     * Null target uses A2014's scalar look_active=0 without calling this. */
    int (*face_actor)(void *, SmN64ComboOwner *, uint32_t actor);
    int (*stop)(void *, SmN64ComboOwner *); /* Original 97DE4; 1 success. */
    /* Original 7FB08 lifetime lookup. Return 1 found, 0 expired/null, negative
     * unavailable. Expiration clears only grab_actor, as the original does. */
    int (*grab_actor)(void *, uint32_t handle, SmN64GrabActor *);
    /* Actor virtual grab request at completed clip120. Return 1 accepted,
     * 0 rejected, negative unavailable. Actor owns its actual response and any
     * player changes; acceptance alone NEVER invents clip123 or an enemy hold. */
    int (*grab_request)(void *, SmN64ComboOwner *, uint32_t actor,
                        const int32_t hold_position[3]);
    /* Clear source actor3C8 bit0x40. Return 1 success. */
    int (*release_grab)(void *, uint32_t actor);
    /* Original 989D0(player,0), synchronous. Return 1 success. */
    int (*move_grab)(void *, SmN64ComboOwner *);
} SmN64ComboOwnerHost;

/* Original state0x800 owner (8E940–8EB10), including dispatcher speed reset.
 * Ground precedes jump; early web+held punch/kick has a strict elapsed<5 gate.
 * Status2/3/6 transition to trap/yank/impact after the interpreter returns.
 * This is an owner composition API: it does not substitute missing world,
 * animation, enemy, root/contact or RNG owners. Return 1 handled, 0 other state,
 * -1 invalid scalar/dependency, -2 unavailable/failed host boundary.
 * Errors after callbacks are fail-stop or whole-owner rollback boundaries;
 * local struct rollback alone cannot undo actor effects or shared RNG. */
int smn64_combo_owner_step(SmN64ComboOwner *, SmN64CombatInput *,
    const SmN64WebButtons *held, uint32_t tick, const uint16_t *counts, size_t,
    const SmN64ComboOwnerHost *);

/* Admission for the early held web+attack GRAB request after the original
 * lost-ground/jump precedence. Decline consumes the recognized request and
 * continues the current combo, without selecting a substitute command. */
int smn64_combo_owner_step_admitted(SmN64ComboOwner *,SmN64CombatInput *,
    const SmN64WebButtons *,uint32_t,const uint16_t *,size_t,
    const SmN64ComboOwnerHost *,SmN64CombatAdmission,void *);
int smn64_grab_owner_step_admitted(SmN64ComboOwner *,const SmN64CombatInput *,
    uint16_t,uint32_t,int32_t,const uint16_t *,size_t,
    const SmN64ComboOwnerHost *,SmN64CombatAdmission,void *);

/* Original state0x2000000 grab continuation (8E70C–8E93C). completed_animation
 * is the original generic animation-successor result, 0xffff when none.
 * Difficulty is source F5EF4: 0/1/2/other -> special actor timeout420/120/120/60.
 * Subsequent mounted punch/kick states0x8000000/0x4000000 remain separate owners.
 * An expired target with zero elapsed during clip123 would dereference null in
 * original code; this API fails explicitly rather than reproducing that fault. */
int smn64_grab_owner_step(SmN64ComboOwner *, const SmN64CombatInput *,
    uint16_t completed_animation, uint32_t tick, int32_t difficulty,
    const uint16_t *counts, size_t, const SmN64ComboOwnerHost *);
#endif
