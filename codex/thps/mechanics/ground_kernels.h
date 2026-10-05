#ifndef THPS1_GROUND_KERNELS_H
#define THPS1_GROUND_KERNELS_H
#include <stdint.h>
/* Pure original-USA-Rev1 arithmetic slices; caller owns contact/animation gates.
 * Q12 positions/velocities; dt is Q8 frames relative to 30 Hz (256 default).
 * These are source reconstructions, not a standalone simulation. */
int32_t thps1_ground_speed_from_total(int32_t total);
int32_t thps1_ground_steer_step(int32_t dt_q8, int32_t equipment_mode, int down);
int32_t thps1_ground_steer_rate(int32_t old, int32_t step, int32_t limit,
                             int32_t analog_x, int left, int right, int down);
int32_t thps1_ground_brake_threshold(int32_t normal_y_q12);
int32_t thps1_ground_brake_component(int32_t velocity, int32_t analog_y, int32_t dt_q8);
int32_t thps1_ground_integrate_position(int32_t position,int32_t velocity,int32_t acceleration,int32_t dt_q8,int32_t dt_squared_q8);
int32_t thps1_ground_integrate_velocity(int32_t velocity,int32_t acceleration,int32_t dt_q8);
int32_t thps1_ground_cap_component(int32_t velocity,int32_t cap,int32_t speed);
#endif
