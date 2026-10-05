#ifndef CODEX_BM64_MOVEMENT_H
#define CODEX_BM64_MOVEMENT_H

#include <stdint.h>

/* Original Bomberman 64 US 1.0 arithmetic in original world units per update.
 * This is a bounded normal on-foot slice, NOT the game's collision solver,
 * scheduler or complete player state machine.
 * No manual jump/button API: the examined normal movement path has none.
 */
#define BM64_PLAYER_GRAVITY 0x1.a22224p+0f /* ROM float 0x3FD11112 */
#define BM64_PLAYER_TERMINAL_FALL 100.0f
#define BM64_PLAYER_RADIUS 30.0f
#define BM64_PLAYER_HEIGHT 51.0f
#define BM64_PLAYER_TOP_OFFSET 81.0f
#define BM64_PLAYER_INPUT_BLOCK_MASK UINT32_C(0x02000C20)
#define BM64_PLAYER_TRANSLATION_BLOCK_MASK UINT32_C(0x00600000)
#define BM64_DIRECTION_NONE 1u

typedef struct Bm64PolarInput {
    float magnitude;       /* magnitude AFTER original per-axis deadzone trim */
    float angle_degrees;   /* 0=+Z, 90=+X, 180=-Z, 270=-X */
} Bm64PolarInput;

typedef struct Bm64MotionPlan {
    float speed;           /* original scalar tier: 0, 2, 4, or 8.5 */
    float dx;
    float dz;
    float facing_target;   /* meaningful only when direction != NONE */
    uint32_t direction;    /* original encoded octant; 1 = no direction */
} Bm64MotionPlan;

typedef struct Bm64Facing {
    float angle;
    float target_unwrapped;
    float angle_unwrapped;
    float step;
    int turning;
} Bm64Facing;

typedef struct Bm64Vertical {
    float y;
    float fall_velocity;  /* +down, unlike SM64 velocity Y */
    float extra_y;        /* original LevelClass+0x68 */
} Bm64Vertical;

enum Bm64VerticalContact {
    BM64_CONTACT_NONE = 0,
    BM64_CONTACT_FLOOR = 1,
    BM64_CONTACT_CEILING = 2
};

/* The original controller sample is signed 8-bit; deadzone default is 3.
 * Uses the recovered original atan continued fraction, original double
 * conversion constant, and sqrtf for the original hardware sqrt.s.
 */
Bm64PolarInput bm64_controller_polar(int8_t stick_x, int8_t stick_y);

/* Camera rotation/mirroring is an adapter responsibility; supply world angle
 * in [0,360). Nonfinite/out-of-range input safely produces no motion.
 * Virus, ice, carried-object speed effects, damage, slopes and cutscenes are
 * outside this normal-state function. Input/movement flag gates are original.
 */
Bm64MotionPlan bm64_plan_normal_motion(float magnitude, float world_angle,
                                     uint32_t player_flags);

/* Original turning defers new targets while an existing turn is in progress.
 * Initialize with bm64_facing_init on spawn/teleport; call once per update.
 */
void bm64_facing_init(Bm64Facing *state, float angle);
void bm64_facing_tick(Bm64Facing *state, const Bm64MotionPlan *plan);

/* Original 0x80230914 arithmetic given floor/ceiling heights from the host.
 * Run AFTER horizontal collision resolution. The caller decides whether this
 * player state runs gravity. Floors/ceilings here are local planar contacts;
 * the original grid lookup, slopes, platform/bomb contacts are not reproduced.
 * Gravity includes the original one-update terminal overshoot behavior.
 */
unsigned bm64_vertical_tick(Bm64Vertical *state,
                            int has_floor, float floor_y,
                            int has_ceiling, float ceiling_y);
#endif
