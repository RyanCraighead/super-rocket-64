#ifndef BM64_BOMB_KERNEL_H
#define BM64_BOMB_KERNEL_H
#include <stddef.h>
#include <stdint.h>
/* Portable scalar projection of verified US 1.0 routines, not the N64 ABI.
 * Original simulation ticks and BM64 world units. No SM64 unit conversion.
 * See PROVENANCE.md for exact instruction ranges and unported dependencies. */
enum {
 BM64_BOMB_RED=0x0001, BM64_BOMB_REMOTE=0x0002,
 BM64_BOMB_FULL_POWER=0x0100, BM64_BOMB_DEFAULT=0x0200,
 BM64_BOMB_SHORT_FUSE=0x0400,
 BM64_BOMB_HUGE=0x0800
};
#define BM64_INITIAL_BOMB_COUNT 2 /* 80277e40 */
#define BM64_INITIAL_PLAYER_FIRE_POWER 2 /* 80277e5c, spawned bomb adds1 */

enum {
 BM64_BOMB_EXPLODING=0x0001, BM64_BOMB_FUSE_ACTIVE=0x0002,
 BM64_BOMB_ROLLING=0x0004, BM64_BOMB_CONTACT_ACTIVE=0x0008,
 BM64_BOMB_HELD=0x0010, BM64_BOMB_DEFLATING=0x0020,
 BM64_BOMB_SCALE_LIMIT=0x0040, BM64_BOMB_PUMPED=0x0080,
 BM64_BOMB_AIRBORNE=0x0200, BM64_BOMB_INTERACTED=0x0400,
 BM64_BOMB_GROUNDED=0x0800
};
enum {
 BM64_BOMB_EVENT_SOUND_16=1u<<0, BM64_BOMB_EVENT_SOUND_17=1u<<1,
 BM64_BOMB_EVENT_SOUND_1A=1u<<2, BM64_BOMB_EVENT_PUMP_PARTICLE=1u<<3,
 BM64_BOMB_EVENT_STOP_PARTICLE=1u<<4,
 BM64_BOMB_EVENT_FULL_PUMP=1u<<5
};
typedef struct Bm64Bomb {
 int16_t type, state, owner, fire_power;
 int16_t fuse, pump_charge, pump_stage, pump_sound_timer1, pump_sound_timer2;
 int16_t heading, particle_age;
 int32_t particle_id;
 float scale, scale_min, scale_max, scale_step;
 float radius, height;
 float position[3];
} Bm64Bomb;
typedef struct Bm64BombThrowPreset {
 float lift;                 /* LevelClass+0x68, added to Y each tick */
 float downward_velocity;    /* LevelClass+0x6c, subtracted from Y */
 float planar_speed;         /* ThrowState+0x1c */
 float gravity;              /* LevelClass+0x70 */
} Bm64BombThrowPreset;
/* Allocator/render/list side effects are deliberately outside this projection. */
void bm64_bomb_init(Bm64Bomb *bomb, int16_t type, int16_t owner, int32_t player_fire_power);
int bm64_bomb_model_id(int16_t type);
void bm64_bomb_pump_input(Bm64Bomb *bomb);
/* Call visual_tick before fuse_tick, as 0x8027723c does. blocked_full_pump is
 * original collision query 0x80232d18(level,0x10) != 0 after radius/height=70. */
uint32_t bm64_bomb_visual_tick(Bm64Bomb *bomb, int16_t pause_flag, int blocked_full_pump);
void bm64_bomb_fuse_tick(Bm64Bomb *bomb, int16_t pause_flag);
/* Original update commits scale-limit bit to explosion after motion/collision. */
int bm64_bomb_commit_scale_explosion(Bm64Bomb *bomb);
/* 8027130c: pumped bombs are refused; success pauses/resets fuse.
 * Host detaches airborne/held state and roll queue before attachment. */
int bm64_bomb_pickup(Bm64Bomb *bomb);
void bm64_bomb_release(Bm64Bomb *bomb);
/* Pass original linked-list traversal order. Only the first eligible bomb is set. */
int bm64_bomb_detonate_first(Bm64Bomb *bombs, size_t count, int16_t owner, int system_enabled);
/* Returns whether a rolling bomb changed; host removes it from player's roll queue. */
int bm64_bomb_stop_rolling(Bm64Bomb *bomb);
/* Modes 0..3 only; unknown modes have undefined vertical data in the original. */
int bm64_bomb_throw_preset(int16_t mode, Bm64BombThrowPreset *out);
/* Original degree quantization, including one-wrap normalization and cutoffs. */
int bm64_bomb_heading_from_degrees(float angle);
/* Source explosion component scalar animation. Core sphere components are0..2;
 * decorative components3..8 retain their original scale multipliers. */
typedef struct Bm64Explosion {
 int16_t component, fire_level, angle, angle_step, delay;
 int16_t opacity, alive, damaging;
 float amplitude, scale, rotation;
} Bm64Explosion;
int bm64_explosion_model_id(int16_t component,int16_t bomb_type);
int bm64_explosion_init(Bm64Explosion *e,int16_t component,int16_t bomb_type,
                        int16_t fire_level,int16_t start_delay);
/* Supply original sin/cos of angle+angle_step (degrees). If delay>0 they are
 * ignored. Source collision checks occur BEFORE animation advancement. */
void bm64_explosion_tick(Bm64Explosion *e,float sine_next,float cosine_next);
/* Uses the shared ROM-derived sin/cos implementation. Preferred host API. */
void bm64_explosion_tick_original(Bm64Explosion *e);
float bm64_explosion_hit_radius(const Bm64Explosion *e);
/* Original flat-ground direction projection from80272024/80291944.
 * No terrain slope, contact, reflection or floor bounce is implied. */
void bm64_bomb_flat_delta(int16_t heading,float speed,float *dx,float *dz);
/* Original attachment-field projection. local[] corresponds to LevelClass
 * offsets4c/50/54; residual fields match HeldObject1c/20/24/28/2c.
 * Host must reproduce source attachment transform modes, not treat all local
 * values as a free-standing world position throughout the20-tick lift. */
typedef struct Bm64HoldTween {
 int16_t timer;
 float residual_x, phase, residual_z, previous_wave, lift_distance;
 float local[3];
} Bm64HoldTween;
void bm64_hold_begin(Bm64HoldTween *h,const float player[3],const float bomb[3],
                     float facing,float player_radius,float player_height,
                     float bomb_radius);
void bm64_hold_tick(Bm64HoldTween *h,float facing,float player_height,float bomb_radius);
#endif
