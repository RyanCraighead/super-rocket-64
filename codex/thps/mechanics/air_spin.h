#ifndef THPS1_AIR_SPIN_H
#define THPS1_AIR_SPIN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Air-only HandleSpin, original THPS1 N64 USA Rev1 0x8004fd90..80050d68.
 * Call only with state != 0. There is intentionally no invented spin-stat
 * multiplier: this routine does not read a profile or equipment in air.
 * Angles use 4096/turn. Rates are Q12 source-angle units per source tick.
 */
typedef struct Thps1AirSpin {
 int32_t state;                  /* object +0x82c */
 int32_t input_lock, count_lock; /* +0x37c,+0x380; semantics not fully named */
 int32_t queued180, queued_angle, completed180; /* +0x41c,+0x420,+0x424 */
 int32_t no_input_since;         /* +0x350 */
 int32_t strong_decay, turning;  /* +0x410,+0x414 */
 int32_t yaw_rate, lean_rate, pitch_delta; /* +0x930,+0x934,+0x92c */
 int32_t crouched, flags;        /* +0x834,+0x208 */
 int32_t animation, frame, animation_rate; /* +0x21e,+0x21c,+0x228 */
} Thps1AirSpin;
typedef struct Thps1AirSpinInput {
 int32_t dt8, tick;              /* globals 0x800de99c,0x800de994 */
 int32_t analog_x, analog_y;     /* signed bytes object +0x98c,+0x98d */
 uint8_t held40, held60, held80, held90;
 uint8_t held10, held20, helda0, heldb0;
 uint8_t edge51, edge71;         /* consumed in-place only in states1/2 */
 uint8_t global40, global60;     /* global controller 0x800d49e0, not object */
} Thps1AirSpinInput;
typedef struct Thps1AirSpinOutput {
 int32_t queued_rotation;       /* call 0x80058594(delta,0,0), source angle */
 int32_t rotation_calls;
 int32_t animation_changed;     /* call orientation helper, then Anim::Run */
 int32_t animation, from, to, continuation;
} Thps1AirSpinOutput;
/* Returns0 without changing anything for ground state0. Otherwise returns1.
 * Output is reset every accepted call. Animation commands must be applied with
 * original frame counts (air_animation). The rotation/matrix helper and
 * pending stance switch helper remain external boundaries; no host heading,
 * momentum, profile, or score is modified by this controller.
 */
int thps1_air_spin(Thps1AirSpin *, Thps1AirSpinInput *, Thps1AirSpinOutput *);
/* Later air-physics step0x80054600..546b8: call once only when applying
 * continuous source yaw (original zero-vector gate passed). Does not rotate a
 * matrix, apply dt again, or change the queued180 count. Returns yaw_rate>>12.
 * Skip this when the host blocks rotation, since the original skips accounting.
 */
int32_t thps1_air_spin_accumulate_yaw(Thps1AirSpin *);
/* Exact original22fcc/41f80/9c914/41ec0 float32 matrix product. Both
 * arguments are the source884 layout, row-major3x3 signed Q12. Columns are
 * source side(8dc), up(8e8), longitudinal(8d0), not renderer/travel basis.
 * rotation is the original22598 result (private exporter); neither function
 * computes host sin/cos or changes velocity. Pitch also preserves local pivot
 * (0,70,0), matching original544e8..545c4; position is source Y-down Q12.
 */
void thps1_air_spin_rotate_basis_q12(int16_t basis[9],const int16_t rotation[9]);
void thps1_air_spin_pitch_basis_q12(int16_t basis[9],int32_t position[3],const int16_t rotation[9]);
#ifdef __cplusplus
}
#endif
#endif
