#ifndef ROCKET_BINDINGS_H
#define ROCKET_BINDINGS_H
#include <stdio.h>
#include "../../codex/rocketleague/input/gamepad.h"

/* Stable local config IDs, never SDL device indices or network fields. */
enum RocketBinding {
    RB_NONE, RB_SOUTH, RB_EAST, RB_WEST, RB_NORTH, RB_LB, RB_RB,
    RB_LS, RB_RS, RB_UP, RB_DOWN, RB_LEFT, RB_RIGHT, RB_LT, RB_RT, RB_COUNT
};
enum RocketAction {
    RA_THROTTLE, RA_BRAKE, RA_JUMP, RA_BOOST, RA_SLIDE, RA_ROLL,
    RA_ROLL_LEFT, RA_ROLL_RIGHT, RA_CAMERA, RA_COUNT
};
typedef struct RocketBindings {
    unsigned int action[RA_COUNT];
    unsigned int stick, invert_x, invert_y;
} RocketBindings;
typedef struct RocketPadSample {
    /* SDL standardized button bits. Back/Start/Guide/Touchpad are reserved. */
    uint32_t buttons;
    int16_t left_x, left_y, right_x, right_y, left_trigger, right_trigger;
} RocketPadSample;

#ifdef __cplusplus
extern "C" {
#endif
extern RocketBindings configRocketBindings;
extern const RocketBindings rocket_default_bindings;
extern const char *const rocket_binding_names[RB_COUNT];
extern const char *const rocket_action_names[RA_COUNT];
void rocket_bindings_reset(void);
int rocket_bindings_valid(const RocketBindings *bindings);
/* Shared buttons are intentional and permitted; return a bit per shared action. */
unsigned int rocket_bindings_conflicts(const RocketBindings *bindings);
void rocket_bindings_read(char **tokens, int count);
int rocket_bindings_parse(char **tokens, int count, RocketBindings *result);
void rocket_bindings_write(FILE *file);
void rocket_bindings_apply(const RocketBindings *bindings, const RocketPadSample *raw, RocketGamepad *pad);
int rocket_bindings_neutral(const RocketPadSample *raw);
#ifdef __cplusplus
}
#endif
#endif
