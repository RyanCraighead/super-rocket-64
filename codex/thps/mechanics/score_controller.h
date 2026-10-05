#ifndef THPS1_SCORE_CONTROLLER_H
#define THPS1_SCORE_CONTROLLER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define THPS1_SCORE_TRICKS 80
#define THPS1_SCORE_ENTRIES 20
/* Source ordinary single-player score arithmetic, default game_mode 0.
 * Twenty records exist, but source count saturates at 19; slot19 is scratch.
 * Names/rendering, audio, rumble, challenge callbacks and career are excluded. */
typedef struct Thps1Score {
 int32_t enabled, game_mode, blocked, score_mode;
 int32_t repetitions[THPS1_SCORE_TRICKS], attempt_repetitions[THPS1_SCORE_TRICKS];
 int32_t entry_points[THPS1_SCORE_ENTRIES], entry_index[THPS1_SCORE_ENTRIES];
 /* Name disambiguation only: pure-spin74 shares crooked-grind74. */
 int32_t entry_spin_degrees[THPS1_SCORE_ENTRIES];
 int32_t count;
 uint32_t settled_total, pending, best_combo;
 int32_t last_base, last_multiplier2;
 uint32_t last_award, last_bail;
 int32_t meter, special;
 /* Original bank-delay40. Lists remain in entry arrays after count clears.
  * last_count and result are host-readable snapshots; result1 bank,2 bail. */
 int32_t bank_delay, last_count, result;
 int32_t hud_active, hud_bailed;
} Thps1Score;
void thps1_score_init(Thps1Score *);
/* Call only at source begin-attempt4c9e8 boundary (source state!=5, timer!=0).
 * It preserves live list and session repetition, and clears rollback counts. */
void thps1_score_begin(Thps1Score *);
/* Returns depreciated points. Negative base means positive nondepreciating
 * points, as used by source pure spins. Invalid index is safely ignored. */
int32_t thps1_score_add(Thps1Score *,int32_t index,int32_t base);
/* Original4638c adds directly to the last COUNTED entry, without depreciation.
 * Empty-list calls are rejected rather than reproducing out-of-bounds writes. */
void thps1_score_hold(Thps1Score *,int32_t delta);
/* Existing repetition count, no increment. Air holds divisor20, grind5. */
int32_t thps1_score_hold_value(const Thps1Score *,int32_t index,int32_t base,int32_t divisor);
int32_t thps1_score_base(const Thps1Score *);
int32_t thps1_score_multiplier2(int32_t count,int32_t half_turns);
uint32_t thps1_score_preview(const Thps1Score *,int32_t half_turns);
uint32_t thps1_score_total(const Thps1Score *);
int32_t thps1_score_landing_boost(const Thps1Score *);
/* Deferred successful landing4cc10, before owner clears spin/trick state.
 * suppress_round is source+380, previous_state+830, scored+45c.
 * Caller separately clears owner fields as documented in landing_evidence. */
uint32_t thps1_score_bank(Thps1Score *,int32_t half_turns,int32_t residual,
                        int32_t previous_state,int32_t scored,int32_t suppress_round);
/* Original4ced0 rollback, list loss and meter reset. No total-score penalty. */
void thps1_score_bail(Thps1Score *);
/* Source5a1f4..5a224, once per active physics tick, before trick selection. */
void thps1_score_tick(Thps1Score *);
/* Source44490..44540 then44930..44940: once per unpaused HUD update.
 * Pending counts down into settled total only after bank_delay expires. */
void thps1_score_display_tick(Thps1Score *);
#ifdef __cplusplus
}
#endif
#endif
