#ifndef SMN64_CHARACTER_H
#define SMN64_CHARACTER_H
#include "actor_bridge.h"
#include "combat_owner_n64.h"
#include "../climbing/controller_n64.h"
#include "../contact/contact_n64.h"
#include "../markers/markers_n64.h"
#include "../web/lifecycle_n64.h"
#define SMN64_BUTTON_WEB 1u
#define SMN64_BUTTON_KICK 2u
#define SMN64_BUTTON_PUNCH 4u
#define SMN64_BUTTON_JUMP 8u
#define SMN64_BUTTON_AIM 16u
#define SMN64_BUTTON_ZIP 32u
#define SMN64_BUTTON_SWING 64u
typedef struct SmN64CharacterInput {
    SmN64Input motion;uint32_t pressed,held;
    int32_t camera_position[3]; /* native whole units, explicit host camera */
    float effect_seconds; /* source effect clock, not render refresh delta */
} SmN64CharacterInput;
typedef struct SmN64Character {
    SmN64ClimbState actor; /* authoritative full basis and common actor state */
    SmN64Player ground;
    SmN64FreeContact contact;
    SmN64WebRuntime web;
    SmN64WebOwner web_owner;
    SmN64WebVisuals visuals;
    SmN64WebEvents web_events;
    SmN64CharacterCombat combat;
    SmN64Marker markers[9];
    int16_t retained_pose[216];
    int32_t body_translation[3]; /* retained before current physics displacement */
    uint32_t ticks,falling_tick;
    int32_t stable_updates;
    int16_t maximum_health;
    int32_t source_level;
    SmN64ClimbTransitionEvent climb_event;
    uint64_t graphical_clock; /* transactional shared primitive creation order */
} SmN64Character;
typedef struct SmN64CharacterServices {
    void *context;
    SmN64FreeTrace free_trace;
    SmN64ClimbTrace trace;
    int (*pose)(void *,int clip,int frame,int16_t out[216]);
    /* Optional source-ordered action integration. Receives only a pending
     * character transaction; world changes must be queued until it commits. */
    int (*action)(void *,SmN64Character *,SmN64ClimbState *,uint32_t source_address,
                  const SmN64CharacterInput *,const uint16_t *counts,size_t);
    const SmN64CombatOwnerServices *combat;
    /* Optional host delivery point for actual queued attacks, before actor
     * animation/player-AI. Callback mutates only this pending transaction. */
    int (*incoming)(void *,SmN64Character *,SmN64CombatOwnerFrame *);
    int (*trails_retain)(void *,const SmN64ClimbState *,const SmN64Marker *,size_t,
                         const int16_t pose[216],const int32_t body[3],uint32_t tick);
    int (*trails_effects)(void *,uint32_t tick);
    /* Original projectile sprite pass5534 precedes traversal strand555C RNG.
     * Receives current pending aliases after final pose/anchors/held updates.
     * Mutations and native/effect allocations must join the whole transaction. */
    int (*attack_effects)(void *,SmN64CombatOwnerFrame *,SmN64CharacterCombat *);
} SmN64CharacterServices;
/* Explicit Normal difficulty2, ordinary suit0, fresh profile at source level
 * 0x100. Host position/yaw/seed are explicit cross-game initialization policy.
 * All nine marker records and all300 clip counts must come from verified input. */
int smn64_character_init(SmN64Character *,const int32_t position[3],uint16_t yaw,
    uint32_t seed,const SmN64Marker markers[9],const uint16_t *counts,size_t,
    const SmN64CharacterServices *);
/* Ground/air + adhered source owners, including held source move/stop/jump and
 * common-tail constraints. Carry object reads and release operations use the
 * optional combat services against this same pending whole-character copy.
 * Expired host handles clear their owning aliases without fabricated physics.
 * Negative leaves entire actor unchanged.
 * This integration boundary is still being extended with full web/combat state
 * owners. Unknown active state=-30; unavailable pressed/held actions=-20.
 * Never substitute Mario motion after a negative return. */
int smn64_character_tick(SmN64Character *,const SmN64CharacterInput *,
    const uint16_t *counts,size_t,const SmN64CharacterServices *);
#endif
