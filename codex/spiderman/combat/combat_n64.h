#ifndef SMN64_COMBAT_H
#define SMN64_COMBAT_H
#include <stddef.h>
#include <stdint.h>
#include "../movement/n64_behavior.h"
#include "command_n64.h"

/* Source-authored data must be decoded from the user's verified USA 1.0 boot
 * using extract_combat.py. No tables are hardcoded into this implementation. */
typedef struct SmN64ComboBranch { uint16_t target, at, offset, animation, flag; } SmN64ComboBranch;
typedef struct SmN64ComboDef {
    uint16_t present, id, animation, damage, hit_start, hit_end;
    uint16_t normal_start, normal_end, alternate_start, alternate_end;
    uint16_t impulse, duration, hit_flags, alternate;
    uint16_t branch_count, sequence_count, bone_count, frame_count;
    SmN64ComboBranch branches[16];
    uint8_t sequence[128], bones[32], frames[256];
} SmN64ComboDef;
typedef struct SmN64RootDef { uint16_t count; int8_t values[256]; } SmN64RootDef;
typedef struct SmN64CombatBank { SmN64ComboDef combo[32]; SmN64RootDef root[300]; uint8_t air_types[21]; } SmN64CombatBank;
int smn64_combat_bank_load(SmN64CombatBank *, const uint8_t *, size_t);

typedef struct SmN64ComboCandidate {
    uint32_t active, first, alternate, web_prefix, last_tick, flags, cursor;
} SmN64ComboCandidate;
typedef struct SmN64Combo {
    SmN64Anim anim;
    uint32_t started, last_input;
    uint16_t id, active, matching, pending_at, pending_offset, transition_animation, root_cursor;
    int16_t pending_id;
    SmN64ComboCandidate candidate[16];
    uint32_t touched[4];
    uint32_t touched_count, first_pose;
    int32_t prior_bones[32][3], bones[32][3];
    uint32_t glove_hits;
} SmN64Combo;
/* Inputs are the original logical controller bytes. tick consumes pressed bits.
 * Pressed mask: bit0 web, bit1 kick, bit2 punch, bit3 jump. */
typedef struct SmN64CombatInput { uint32_t pressed; uint8_t web_held; } SmN64CombatInput;
typedef struct SmN64CombatMotion {
    int32_t forward_units;
    uint32_t anchored, anchor_bone, include_y; /* anchor_bone is authored MARKER ID6/5/1/0, not joint ID */
} SmN64CombatMotion;
typedef struct SmN64ComboResult {
    int32_t status; /* 0 ended, 1 ongoing, >1 successor ID (source convention) */
    uint32_t began, sample_bones, hit_active;
    SmN64CombatMotion motion;
} SmN64ComboResult;
/* The source selects/faces nearby targets before begin; that contact boundary
 * is independent of the exact authored timing/sequence interpreter here. */
int smn64_combo_begin(SmN64Combo *, const SmN64CombatBank *, uint16_t id,
                     uint32_t tick, uint16_t offset, const uint16_t *counts, size_t count);
int smn64_combo_tick(SmN64Combo *, const SmN64CombatBank *, SmN64CombatInput *,
                    uint32_t tick, const uint16_t *counts, size_t count, SmN64ComboResult *);

/* Native integration: resolve root motion before input may select another clip.
 * Callback sees the source frame/clip. It refreshes pose/body translation, runs
 * actor/world queries and commits only the source-approved displacement.
 * 0blocked and1moved both continue the combo. Negative restores this function's
 * frame/cursor changes without consuming input; the callback must not commit
 * host position/state on failure, or the host must roll it back before retry. */
typedef int (*SmN64ComboMotionResolve)(void *,const SmN64Combo *,const SmN64CombatMotion *);
int smn64_combo_tick_resolved(SmN64Combo *,const SmN64CombatBank *,SmN64CombatInput *,
    uint32_t tick,const uint16_t *counts,size_t,SmN64ComboResult *,
    SmN64ComboMotionResolve,void *context);

/* Optional typed admission exactly when an authored command sequence has
 * completed, BEFORE a pending successor/transition animation is queued. A
 * deliberate decline consumes that request, ends matching for this combo, and
 * lets its existing motion/contacts/ending continue. No alternative command is
 * selected. NULL admission is the original API, including callback ordering. */
int smn64_combo_tick_admitted(SmN64Combo *,const SmN64CombatBank *,SmN64CombatInput *,
    uint32_t tick,const uint16_t *counts,size_t,SmN64ComboResult *,
    SmN64ComboMotionResolve,void *resolve_context,SmN64CombatAdmission,void *admit_context);
SmN64CombatCommand smn64_combo_command(uint16_t id);

/* Contact integration contract. Actors stay in original list order; no synthetic
 * hit is created. distance is native source 0x800AB134 distance, not host radius.
 * Required pose and swept contact callbacks must fail explicitly if unsupported.
 * Source hit testing uses previous bone point -> current + ((current-previous)>>1),
 * radius 0x2000 fixed12. apply receives only geometrically verified contacts.
 * The first geometric contact ends actor iteration even when damage is rejected;
 * it is still entered in the source's four-contact cache. Bone must return
 * exactly1 on success; any other result means the source pose is unavailable.
 * Sampling failure leaves bone history untouched. A later sweep/apply failure
 * can follow committed host effects: fail-stop or roll back the whole owner,
 * rather than blindly retrying that tick. */
typedef struct SmN64CombatActor { uint32_t id; uint16_t flags, type; int32_t distance; } SmN64CombatActor;
typedef struct SmN64CombatHit {
    uint32_t actor, kind;
    uint16_t damage, impulse, duration;
    uint8_t flags, hit_part;
    int32_t position[3], direction[3];
} SmN64CombatHit;
typedef struct SmN64CombatHost {
    void *context;
    int (*bone)(void *, uint8_t bone, int32_t position[3]);
    int (*sweep)(void *, uint32_t actor, const int32_t from[3], const int32_t to[3],
                 int32_t radius, uint8_t *hit_part, int32_t contact_position[3]);
    int (*apply)(void *, const SmN64CombatHit *);
} SmN64CombatHost;
int smn64_combo_contacts(SmN64Combo *, const SmN64CombatBank *, uint32_t tick,
                        const SmN64CombatActor *, size_t actor_count,
                        uint8_t suit, int32_t difficulty, const SmN64CombatHost *);
/* Exact scoring/predicate slice of 0x800A1E18. Supplied facing is the source's
 * normalized negative local Z, and visible is the source line-of-sight result. */
typedef struct SmN64CombatTarget {
    uint32_t id; uint16_t flags, enabled;
    int32_t distance, facing; uint8_t visible;
} SmN64CombatTarget;
void smn64_combat_normalize(const int32_t src[3],int32_t dst[3]);
uint32_t smn64_combat_select_target(const SmN64CombatTarget *, size_t,
                                  int32_t max_distance, int32_t min_facing,
                                  int32_t distance_weight, int32_t facing_weight);
#endif
