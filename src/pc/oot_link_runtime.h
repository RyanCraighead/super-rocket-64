#ifndef SM64_OOT_LINK_RUNTIME_H
#define SM64_OOT_LINK_RUNTIME_H
#include <stdint.h>
#include "../../codex/oot/combat/oot_sword.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Single local player, game-thread only. Bounded original 20 Hz player slice,
 * translated into native collision. World scale is an adapter choice, 2.5.
 * This loader reads local extraction artifacts; it never writes or downloads. */
#define OOT_LINK_WORLD_SCALE 2.5f
int oot_link_runtime_init(const char *asset_directory);
void oot_link_runtime_shutdown(void);
int oot_link_runtime_enabled(void);
int oot_link_runtime_visible(void); /* a real pose has actually drawn */
const char *oot_link_runtime_status(void);
/* Read-only bounded diagnostics for native QA. No pointers into runtime state,
 * no pose bytes, control input, or ability to change the source simulation. */
typedef struct OotLinkSnapshot {
    int enabled, attached, drawable, visible, paused;
    int source_tick, source_frame_complete, attack_id, recovery, weapon_state, active_quads;
    uint64_t source_ticks, swing_id;
    float position[3], source_speed_xz, source_velocity_y;
    int16_t shape_yaw;
    float animation_frame, secondary_frame, secondary_weight;
    char animation[96], secondary_animation[96], asset_profile[32];
} OotLinkSnapshot;
int oot_link_runtime_snapshot(OotLinkSnapshot *snapshot);

void oot_link_runtime_suspend(void); /* forget host attachment and all attacks */
void oot_link_runtime_pause(void);   /* keep pose; cancel attack/input/clock */
/* Reuse the retained source pose after suspension, with native action transform.
 * Does not attach the source controller or enable sword collision. */
int oot_link_runtime_present(const float position[3], int16_t yaw);
int oot_link_runtime_draw(const float view[16], const float projection[16], const int viewport[4]);

typedef struct OotLinkInput {
    float position[3];
    float host_forward_velocity, host_y_velocity;
    float stick_magnitude; /* oot_move_stick_magnitude(rawStickX,rawStickY) */
    int16_t facing_yaw, world_yaw, stick_yaw, floor_pitch;
    int grounded, attack_pressed, attack_held, targeting;
} OotLinkInput;
typedef struct OotLinkStep {
    int source_tick, attacking;
    float displacement[3]; /* Full source actor delta, before action/collision */
    float animation_displacement[3]; /* Root delta computed by commit, host units */
    float host_forward_velocity, host_y_velocity;
    int16_t shape_yaw;
    uint64_t swing_id;
} OotLinkStep;
typedef struct OotLinkCollision {
    float position[3];
    int grounded, left_ground, hit_wall, hit_ceiling;
    float distance_to_floor; /* host units */
    int floor_forbids_jump;
} OotLinkCollision;
/* Pair step/commit once per native 30 Hz physics frame. No work during draws.
 * commit reconciles actor collision then advances the source action/pose.
 * Apply its animation_displacement with native collision, then call finish
 * to update the final pose and generate swept left-hand quads. Never reuse those quads on an intervening 30 Hz frame. */
int oot_link_runtime_step(const OotLinkInput *input, OotLinkStep *step);
void oot_link_runtime_commit(const OotLinkCollision *collision, OotLinkStep *step);
void oot_link_runtime_finish(const OotLinkCollision *collision, OotLinkStep *step);
int oot_link_runtime_quads(OotSwordQuad quads[2], uint64_t *swing_id);
#ifdef OOT_LINK_RUNTIME_TESTING
int oot_link_runtime_test_pose(int16_t frame[22][3]);
float oot_link_runtime_test_frame(void);
int oot_link_runtime_test_attack(void);
uint64_t oot_link_runtime_test_ticks(void);
#endif
#ifdef __cplusplus
}
#endif
#endif
