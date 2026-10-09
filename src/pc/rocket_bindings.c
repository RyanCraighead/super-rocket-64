#include "rocket_bindings.h"
#include <string.h>

#define ROCKET_DEFAULT_BINDINGS { { RB_RT, RB_LT, RB_SOUTH, RB_EAST, RB_WEST, RB_WEST, RB_NONE, RB_NONE, RB_NORTH }, 0, 0, 0 }
const RocketBindings rocket_default_bindings = ROCKET_DEFAULT_BINDINGS;
RocketBindings configRocketBindings = ROCKET_DEFAULT_BINDINGS;
const char *const rocket_binding_names[RB_COUNT] = {
    "Unbound", "A / Cross", "B / Circle", "X / Square", "Y / Triangle",
    "LB / L1", "RB / R1", "LS / L3", "RS / R3", "D-pad Up", "D-pad Down",
    "D-pad Left", "D-pad Right", "LT / L2", "RT / R2"
};
const char *const rocket_action_names[RA_COUNT] = {
    "Accelerate", "Brake / Reverse", "Jump", "Boost", "Powerslide",
    "Air Roll (hold)", "Air Roll Left", "Air Roll Right", "Cycle camera"
};
void rocket_bindings_reset(void) { configRocketBindings = rocket_default_bindings; }

int rocket_bindings_valid(const RocketBindings *b) {
    if (!b || b->stick > 1 || b->invert_x > 1 || b->invert_y > 1) return 0;
    for (int i = 0; i < RA_COUNT; ++i) if (b->action[i] >= RB_COUNT) return 0;
    return 1;
}
unsigned int rocket_bindings_conflicts(const RocketBindings *b) {
    unsigned int result = 0;
    if (!rocket_bindings_valid(b)) return 0;
    for (int i = 0; i < RA_COUNT; ++i) for (int j = i + 1; j < RA_COUNT; ++j) {
        if (b->action[i] != RB_NONE && b->action[i] == b->action[j])
            result |= (1u << i) | (1u << j);
    }
    return result;
}

/* A whole, versioned record is committed only after strict range checking.
 * No partial profile or integer wraparound can silently change a mapping. */
int rocket_bindings_parse(char **tokens, int count, RocketBindings *result) {
    RocketBindings next = rocket_default_bindings;
    unsigned int values[RA_COUNT + 3];
    if (count < 2 || !result) return 0;
    int actions = !strcmp(tokens[1], "1") ? RA_CAMERA : !strcmp(tokens[1], "2") ? RA_COUNT : 0;
    if (!actions || count != actions + 5) return 0;
    for (int i = 0; i < actions + 3; ++i) {
        const char *s = tokens[i + 2];
        unsigned int n = 0;
        if (!*s) return 0;
        for (; *s; ++s) {
            if (*s < '0' || *s > '9' || n > RB_COUNT) return 0;
            n = n * 10 + (unsigned int)(*s - '0');
        }
        values[i] = n;
    }
    for (int i = 0; i < actions; ++i) next.action[i] = values[i];
    /* Never make an existing Triangle/Y driving action also switch cameras.
     * Version 2 preserves any deliberately shared or unbound camera choice. */
    if (actions == RA_CAMERA) for (int i = 0; i < actions; ++i)
        if (next.action[i] == RB_NORTH) next.action[RA_CAMERA] = RB_NONE;
    next.stick = values[actions];
    next.invert_x = values[actions + 1];
    next.invert_y = values[actions + 2];
    if (!rocket_bindings_valid(&next)) return 0;
    *result = next;
    return 1;
}
void rocket_bindings_read(char **tokens, int count) {
    rocket_bindings_parse(tokens,count,&configRocketBindings);
}
void rocket_bindings_write(FILE *file) {
    const RocketBindings *b = rocket_bindings_valid(&configRocketBindings) ?
        &configRocketBindings : &rocket_default_bindings;
    fprintf(file, "rocket-bindings: 2");
    for (int i = 0; i < RA_COUNT; ++i) fprintf(file, " %u", b->action[i]);
    fprintf(file, " %u %u %u\n", b->stick, b->invert_x, b->invert_y);
}

static int16_t binding_value(unsigned int binding, const RocketPadSample *raw) {
    /* SDL's stable GameController button enum (not joystick button numbers). */
    static const int buttons[] = { -1, 0, 1, 2, 3, 9, 10, 7, 8, 11, 12, 13, 14 };
    if (binding == RB_LT) return raw->left_trigger;
    if (binding == RB_RT) return raw->right_trigger;
    if (binding == RB_NONE || binding >= sizeof(buttons) / sizeof(buttons[0])) return 0;
    return (raw->buttons & (1u << buttons[binding])) ? 32767 : 0;
}
static int16_t invert_axis(int16_t value) { return value == -32768 ? 32767 : -value; }
void rocket_bindings_apply(const RocketBindings *b, const RocketPadSample *raw, RocketGamepad *pad) {
    if (!rocket_bindings_valid(b)) b = &rocket_default_bindings;
    pad->left_x = b->stick ? raw->right_x : raw->left_x;
    pad->left_y = b->stick ? raw->right_y : raw->left_y;
    if (b->invert_x) pad->left_x = invert_axis(pad->left_x);
    if (b->invert_y) pad->left_y = invert_axis(pad->left_y);
    pad->right_trigger = binding_value(b->action[RA_THROTTLE], raw);
    pad->left_trigger = binding_value(b->action[RA_BRAKE], raw);
    pad->jump = rocket_pad_trigger(binding_value(b->action[RA_JUMP], raw)) > 0;
    pad->boost = rocket_pad_trigger(binding_value(b->action[RA_BOOST], raw)) > 0;
    pad->powerslide = rocket_pad_trigger(binding_value(b->action[RA_SLIDE], raw)) > 0;
    pad->air_roll = rocket_pad_trigger(binding_value(b->action[RA_ROLL], raw)) > 0;
    pad->air_roll_left = rocket_pad_trigger(binding_value(b->action[RA_ROLL_LEFT], raw)) > 0;
    pad->air_roll_right = rocket_pad_trigger(binding_value(b->action[RA_ROLL_RIGHT], raw)) > 0;
    pad->camera = rocket_pad_trigger(binding_value(b->action[RA_CAMERA], raw)) > 0;
}
int rocket_bindings_neutral(const RocketPadSample *raw) {
    /* Both sticks must be released even when switching the steering stick. */
    return !raw->buttons && rocket_pad_axes_neutral(raw->left_x, raw->left_y,
        raw->left_trigger, raw->right_trigger) &&
        rocket_pad_axis(raw->right_x) == 0 && rocket_pad_axis(raw->right_y) == 0;
}
