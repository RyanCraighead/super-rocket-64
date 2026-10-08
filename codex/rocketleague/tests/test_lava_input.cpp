#include <cassert>
#include <cstdio>
#include "pc/rocket_runtime.h"
#include "pc/rocket_bindings.h"
static bool world=true,drawable=true,focused=true;
static RocketGamepad gamepad;
static void *SDL_GetKeyboardFocus(){return focused?(void*)1:nullptr;}
#include "lava-input.inc.cpp"
int main(){
    RocketInput keyboard={},input={};keyboard.throttle=1;keyboard.steer=.5f;
    assert(rocket_runtime_read_input(&keyboard,&input)&&input.throttle==1);
    drawable=false;assert(!rocket_runtime_read_input(&keyboard,&input)&&!input.throttle);
    assert(rocket_runtime_read_selected_input(&keyboard,&input)&&input.throttle==1&&input.steer==.5f);
    focused=false;assert(!rocket_runtime_read_selected_input(&keyboard,&input)&&!input.throttle&&!input.steer);
    focused=true;gamepad.ui_blocked=1;assert(!rocket_runtime_read_selected_input(&keyboard,&input)&&!input.throttle);
    gamepad={};world=false;assert(!rocket_runtime_read_selected_input(&keyboard,&input));world=true;
    assert(!rocket_runtime_read_selected_input(nullptr,&input)&&!rocket_runtime_read_selected_input(&keyboard,nullptr));
    RocketBindings bindings=rocket_default_bindings;bindings.action[RA_THROTTLE]=RB_RB;
    RocketPadSample sample={};sample.buttons=1u<<10;sample.left_x=32767;
    gamepad.connected=gamepad.isolated=1;keyboard={};
    rocket_bindings_apply(&bindings,&sample,&gamepad);
    assert(rocket_runtime_read_selected_input(&keyboard,&input)&&input.throttle==1&&input.steer==-1);
    gamepad.ui_blocked=1;assert(!rocket_runtime_read_selected_input(&keyboard,&input)&&!input.throttle&&!input.steer);
    gamepad={};assert(rocket_runtime_read_selected_input(&keyboard,&input)&&!input.throttle);
    puts("PASS production native-action input reader: draw suspension, focus/menu, remapped digital throttle/steering, disconnect and null gates");
}
