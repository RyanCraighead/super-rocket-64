#ifndef CODEX_BK_MOVEMENT_H
#define CODEX_BK_MOVEMENT_H
#include <stdint.h>

/* Bounded source-derived Banjo-Kazooie on-foot slice; see PROVENANCE.md.
 * Tick exactly once at 30 Hz. All distances are ORIGINAL BK units, velocity
 * units/second, acceleration units/second^2, angles degrees: 0=+Z, 90=+X.
 * Collision, world scale, camera, sound and damage belong to the host. */
#define BK_TICK_SECONDS (1.0f / 30.0f)
#define BK_GRAVITY (-2700.0f)
#define BK_TERMINAL_VELOCITY (-4000.0f)
#define BK_SOURCE_COMMIT "e8d31fa53645616fcac6e051543222b973a2edb0"

enum BkButton { BK_BUTTON_A=1u, BK_BUTTON_B=2u, BK_BUTTON_Z=4u,
                BK_BUTTON_C_LEFT=8u };
/* Original bs_e values, NOT Mario actions. */
enum BkAction {
    BK_IDLE=0x1, BK_WALK_SLOW=0x2, BK_WALK=0x3, BK_RUN=0x4,
    BK_JUMP=0x5, BK_CLAW=0x6, BK_CROUCH=0x7, BK_TROT_JUMP=0x8,
    BK_BUSTER=0xf, BK_FLAP=0x10, BK_PECK=0x11, BK_FLIP=0x12,
    BK_BARGE=0x13, BK_TROT_ENTER=0x14, BK_TROT_IDLE=0x15,
    BK_TROT_WALK=0x16, BK_TROT_EXIT=0x17, BK_CREEP=0x1f,
    BK_LANDING=0x20, BK_FALL=0x2f, BK_ROLL=0x31, BK_TROT_FALL=0x71
};
enum BkModelFlags { BK_MODEL_KAZOOIE_UPPER=1u, BK_MODEL_KAZOOIE_FEET=2u,
                    BK_MODEL_KAZOOIE_DIRECTION=4u };
enum BkEvents { BK_EVENT_TAKEOFF=1u, BK_EVENT_LANDED=2u,
                BK_EVENT_BUSTER_IMPACT=4u };

typedef struct BkStick {
    float magnitude; /* original controller-normalized radial distance [0,1] */
    float angle;     /* local polar angle; host rotates into camera/world */
} BkStick;
typedef struct BkInput {
    float stick_magnitude; /* BEFORE bastick radial deadzone/zone mapping */
    float world_yaw;       /* camera-adjusted direction for nonzero stick */
    uint32_t buttons;      /* held mask; rising edges derived internally */
    int floor_type;       /* BK material 1=.07, 2=.29(default), 3=.15, 4=.05 */
} BkInput;
typedef struct BkMovement {
    float velocity[3];
    float yaw, ideal_yaw, target_yaw, target_speed;
    float gravity, terminal_velocity;
    float elapsed, timer, saved_speed;
    float animation_time, animation_previous, animation_duration;
    float animation_end;
    uint32_t previous_buttons, events;
    uint64_t ticks;
    int action, phase, grounded, used_flap, used_peck;
    int animation_asset, animation_loop, animation_stopped;
    int attack_active, flap_count, from_trot, barge_released;
} BkMovement;
typedef struct BkMotion {
    float delta[3];        /* original BK displacement for this 1/30s tick */
    float velocity[3];     /* original units/sec */
    float yaw;
    float animation_time; /* normalized source animation time [0,1] */
    int animation_asset;  /* original asset ID, not exported table index */
    int action;           /* enum BkAction */
    int airborne;         /* use host air collision resolution if true */
    int attack_active;    /* source hitbox-active window; host applies hits */
    uint32_t model_flags, events;
    uint64_t ticks;
} BkMotion;

/* Includes original integer axis normalization: X 7..59, Y 7..61, /80 steps.
 * Adapter may use magnitude and its own established camera polar convention. */
BkStick bk_controller_stick(int8_t raw_x, int8_t raw_y);
float bk_walk_target_speed(float magnitude);
float bk_trot_target_speed(float magnitude);
void bk_movement_init(BkMovement *state, float yaw, int grounded);
void bk_movement_tick(BkMovement *state, const BkInput *input, BkMotion *out);
/* Call after EVERY host collision solve. on_floor is a resolved floor contact,
 * not proximity to the floor. Rising characters ignore on_floor. Ceiling
 * clears positive Y velocity. Blocked axes clear the corresponding velocity.
 * Contact preserves move-specific landing behavior for the following tick. */
void bk_movement_contact(BkMovement *state, int on_floor, int hit_ceiling,
                         int blocked_x, int blocked_z);
const char *bk_action_name(int action);
#endif
