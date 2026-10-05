#ifndef SM64_THPS_ADAPTER_H
#define SM64_THPS_ADAPTER_H
#include <stdint.h>
struct MarioState;
/* The opt-in adapter owns only its bounded skating states. The ordinary engine
 * retains hazards, warps, menus and its existing collision surfaces. */
int thps_adapter_update(struct MarioState *m);
void thps_adapter_suspend(void);
/* Wheel ownership gate; ordinary launchers retain the default selected state. */
void thps_adapter_set_selected(int selected);
void thps_adapter_pause_inputs(void);
const char *thps_adapter_switch_reason(void);
void thps_adapter_set_window_active(int active);
uint16_t thps_adapter_reserved_buttons(void);
int thps_adapter_controller_active(void);
/* Frame-local SDL-standard spin sideband; cleared at every SDL poll, then
 * published only after focus/menu/wheel/reconnect gates. Keyboard untouched. */
void thps_adapter_set_gamepad_spin(uint8_t mask);
int thps_adapter_render_required(void);
void thps_adapter_render_wait(void);
void thps_adapter_render_success(void);
void thps_adapter_render_failure(const char *reason);
const char *thps_adapter_status(void);
const char *thps_adapter_unavailable_label(void);
const char *thps_adapter_trick_label(void);
int thps_adapter_balance_percent(int *out);
/* Read-only native scoring diagnostics. Integer totals preserve original low32
 * arithmetic; no setter or hidden meter/score mode exists. */
typedef struct ThpsScoreSnapshot {
 uint32_t total, combo, last_award, last_bail, best_combo;
 int32_t base, multiplier2, count, last_count, last_base, last_multiplier2;
 int32_t meter, special, enabled, result, bank_delay, boost_ticks;
 int32_t repetitions[80], attempt_repetitions[80];
 int32_t entry_points[20], entry_index[20], entry_spin_degrees[20];
} ThpsScoreSnapshot;
int thps_adapter_score_snapshot(ThpsScoreSnapshot *out);
/* Caller provides output storage; source markup braces are host-color markers. */
const char *thps_adapter_score_name(int32_t index);
typedef struct ThpsBehaviorSnapshot {
    uint64_t ticks;
    float velocity[3], speed;
    uint32_t events;
    int state, grounded, charge_ticks, clip_slot, frame_index;
    int source_yaw, source_state, bail_active, bail_phase, landing_reason;
    int trick_active, trick_index, trick_interruptible, completed_spins;
    int spin_queued180, spin_yaw_rate, gamepad_spin;
    int grinding, rail_id, balance, mapped_rails;
    uint32_t hidden_joints, stance_flags;
    int32_t source_body_position[3];
} ThpsBehaviorSnapshot;
int thps_adapter_snapshot(ThpsBehaviorSnapshot *out);
#endif
