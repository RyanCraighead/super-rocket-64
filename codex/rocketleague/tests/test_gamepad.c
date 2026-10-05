#include "../input/gamepad.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    RocketInput keyboard={0},out;
    RocketGamepad pad={0};pad.connected=pad.isolated=1;
    // Reported regression: vertical stick must never operate the throttle.
    pad.left_y=-32768;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.throttle==0&&out.pitch==-1);
    pad.left_y=0;pad.right_trigger=32767;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.throttle==1&&out.pitch==0);
    pad.right_trigger=0;pad.left_trigger=32767;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.throttle==-1&&out.pitch==0&&!out.powerslide);
    pad.right_trigger=32767;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.throttle==0);
    pad.left_trigger=0;pad.right_trigger=16384;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.throttle>.4f&&out.throttle<.5f);
    pad.left_x=1000;pad.left_y=-1000;pad.right_trigger=1000;out=rocket_gamepad_merge(&keyboard,&pad);assert(!out.throttle&&!out.steer&&!out.pitch);
    pad.left_x=32767;pad.left_y=0;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.steer==-1&&out.yaw==-1&&!out.roll);
    pad.air_roll=pad.powerslide=1;pad.jump=1;pad.boost=1;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.roll==-1&&!out.yaw&&out.powerslide&&out.jump&&out.boost);
    pad.left_x=-32768;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.steer==1&&out.roll==1);
    pad.air_roll=pad.powerslide=0;pad.left_x=32767;keyboard.steer=-1;keyboard.yaw=-1;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.steer==-1&&out.yaw==-1);keyboard.steer=keyboard.yaw=0;
    pad.jump=pad.boost=pad.air_roll=0;pad.left_x=0;keyboard.throttle=1;keyboard.pitch=-1;keyboard.jump=1;
    out=rocket_gamepad_merge(&keyboard,&pad);assert(out.throttle==1&&out.pitch==-1&&out.jump);
    pad.isolated=0;out=rocket_gamepad_merge(&keyboard,&pad);assert(!out.throttle&&!out.pitch&&!out.jump); // no legacy trigger jump on acquisition
    pad.ui_blocked=1;pad.jump=pad.boost=1;pad.right_trigger=32767;
    out=rocket_gamepad_merge(&keyboard,&pad);assert(!out.throttle&&!out.pitch&&!out.jump&&!out.boost);
    pad.ui_blocked=0;pad.jump=pad.boost=0;pad.right_trigger=0;
    pad.connected=0;out=rocket_gamepad_merge(&keyboard,&pad);assert(out.throttle==1&&out.pitch==-1&&out.jump); // keyboard after unplug
    assert(rocket_pad_axes_neutral(3276,-3276,3277,3277));
    assert(!rocket_pad_axes_neutral(5000,0,0,0));assert(!rocket_pad_axes_neutral(0,-5000,0,0));
    assert(!rocket_pad_axes_neutral(0,0,5000,0));assert(!rocket_pad_axes_neutral(0,0,0,5000));
    puts("PASS gamepad: independent analog triggers/pitch, deadzones, Cross/Circle/Square, yaw/roll, keyboard, takeover and disconnect");
}
