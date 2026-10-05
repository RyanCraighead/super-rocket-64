#ifndef OOT_SWORD_H
#define OOT_SWORD_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A bounded translation of original OoT's one-handed melee rules. The engine
 * supplies original ROM animation tracks and host-world collision responses.
 * See SOURCES.md; these are not a replacement full Player/PlayState port. */
typedef struct { float x, y, z; } OotSwordVec3;
typedef struct { OotSwordVec3 v[4]; bool valid; } OotSwordQuad;
typedef struct { OotSwordVec3 tip, base; bool active; } OotSwordEdgeHistory;
typedef struct {
    int8_t spin_angles[4];
    int8_t directions[4];
    uint8_t index;
} OotSwordStickHistory;
typedef struct {
    int animation;
    int repeat_count;
    int hold_timer;
} OotSwordCombo;
typedef struct {
    int animation;
    const char *track;
    const char *recovery;
    const char *target_recovery;
    uint8_t active_start;
    uint8_t active_end;
} OotSwordAttack;

enum {
    OOT_SWORD_FORWARD = 0, OOT_SWORD_FORWARD_FINISH = 2,
    OOT_SWORD_RIGHT = 4, OOT_SWORD_RIGHT_FINISH = 6,
    OOT_SWORD_LEFT = 8, OOT_SWORD_LEFT_FINISH = 10,
    OOT_SWORD_STAB = 12, OOT_SWORD_STAB_FINISH = 14,
    OOT_SWORD_SPIN = 24,
};

void oot_sword_stick_reset(OotSwordStickHistory *history);
void oot_sword_stick_push(OotSwordStickHistory *history, float magnitude,
                          int16_t stick_angle, int16_t world_yaw, int16_t facing_yaw);
bool oot_sword_can_quick_spin(const OotSwordStickHistory *history);
int oot_sword_select_attack(const OotSwordStickHistory *history, bool z_targeting);
void oot_sword_combo_reset(OotSwordCombo *combo);
void oot_sword_combo_pre_tick(OotSwordCombo *combo);
void oot_sword_combo_release(OotSwordCombo *combo, bool button_held);
int oot_sword_combo_start(OotSwordCombo *combo, int requested_animation);
const OotSwordAttack *oot_sword_attack_info(int animation);
int oot_sword_weapon_state(int animation, float original_frame);
bool oot_sword_animation_once(float *frame, float end_frame, float playback_speed);
void oot_sword_local_edges(float source_length, int *repeat_count,
                          OotSwordVec3 tips[3], OotSwordVec3 bases[3]);
bool oot_sword_sweep(OotSwordEdgeHistory *history, OotSwordVec3 tip,
                     OotSwordVec3 base, OotSwordQuad *quad);
/* Host adaptation: exact triangle clipping against the SM64 target's vertical
 * cylinder, not an invented radial attack area. This collision receiver is
 * native-host code; source OoT supplies the two swept weapon quads. */
bool oot_sword_quad_hits_cylinder(const OotSwordQuad *quad, float center_x,
                                 float bottom_y, float center_z,
                                 float radius, float height);

#ifdef __cplusplus
}
#endif
#endif
