#ifndef CODEX_ROCKET_GAMEPAD_H
#define CODEX_ROCKET_GAMEPAD_H
#include "../physics/rocket_physics.h"
#include <math.h>
/* SDL's standardized layout: south=Cross/A, east=Circle/B, west=Square/X.
 * This is a host mapping, not a claim to reproduce original input processing. */
typedef struct RocketGamepad {
    int connected, isolated, ui_blocked;
    int32_t instance;
    int16_t left_x, left_y, left_trigger, right_trigger;
    int jump, boost, air_roll, powerslide, air_roll_left, air_roll_right;
    int camera; /* Local camera only; never merged into physics/network input. */
} RocketGamepad;
static inline float rocket_pad_axis(int16_t value) {
    float x=value<0?value/32768.f:value/32767.f;
    float magnitude=fabsf(x);
    return magnitude<=.1f?0.f:copysignf((magnitude-.1f)/.9f,x);
}
static inline float rocket_pad_trigger(int16_t value) {
    return value<=3277?0.f:(value/32767.f-.1f)/.9f;
}
/* Resume quarantine must use the same deadzones as the car mapper. */
static inline int rocket_pad_axes_neutral(int16_t x,int16_t y,int16_t lt,int16_t rt) {
    return rocket_pad_axis(x)==0.f && rocket_pad_axis(y)==0.f &&
        rocket_pad_trigger(lt)==0.f && rocket_pad_trigger(rt)==0.f;
}
static inline float rocket_pad_dominant(float keyboard,float pad) {
    return fabsf(keyboard)>fabsf(pad)?keyboard:pad;
}
static inline RocketInput rocket_gamepad_merge(const RocketInput *keyboard,const RocketGamepad *pad) {
    RocketInput out=*keyboard;
    if(pad->ui_blocked){RocketInput neutral={0};return neutral;}
    if(!pad->connected)return out;
    /* The takeover/resume frame can still contain legacy SM64 gamepad input.
     * Discard it for that frame; subsequent isolated frames retain keyboard. */
    if(!pad->isolated){RocketInput neutral={0};out=neutral;}
    out.throttle=rocket_pad_dominant(out.throttle,rocket_pad_trigger(pad->right_trigger)-rocket_pad_trigger(pad->left_trigger));
    // SDL positive X means stick-right. The car's lateral convention is the
    // opposite sign; use it consistently for steering, yaw, roll and dodges.
    out.steer=rocket_pad_dominant(out.steer,-rocket_pad_axis(pad->left_x));
    out.pitch=rocket_pad_dominant(out.pitch,rocket_pad_axis(pad->left_y));
    out.jump=out.jump||pad->jump;out.boost=out.boost||pad->boost;
    int roll_held=out.powerslide||pad->air_roll;
    out.powerslide=out.powerslide||pad->powerslide;
    if(pad->air_roll_left||pad->air_roll_right){
        out.roll=!!pad->air_roll_left-!!pad->air_roll_right;out.yaw=0;
    }else if(roll_held){out.roll=out.steer;out.yaw=0;}else{out.yaw=out.steer;out.roll=0;}
    return out;
}
#endif
