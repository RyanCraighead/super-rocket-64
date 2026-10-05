/* Poll the actual SDL reader with a process-local virtual device. The drawable
 * flag alternates as native door actions suspend/reacquire RocketSim. */
static void trigger(int16_t value){assert(!SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,value));}
static void test_door_input_handoff(void) {
    wheel_enabled=1;rocket_bindings_reset();controller_sdl_rocket_bindings_changed();
    configKeyR[0]=VK_RTRIGGER;configKeyR[1]=VK_BASE_SDL_GAMEPAD+SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    controller_sdl_bind();poll();
    unsigned checks=0;
    for(int pass=0;pass<8;pass++) {
        trigger(32767);
        for(int frame=0;frame<12;frame++) {
            car_drawable=frame==0||frame==11;
            RocketInput input=poll();
            assert(input.throttle==1&&observed.isolated&&!(host_pad.button&R_TRIG));checks++;
        }
        trigger(-32768);assert(poll().throttle==0&&!(host_pad.button&R_TRIG));checks++;
    }
    /* A deliberate unassigned R1 and the camera stick still work in the car. */
    car_drawable=1;button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);poll();assert(host_pad.button&R_TRIG);checks++;
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);poll();assert(!(host_pad.button&R_TRIG));checks++;
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,25000);poll();assert(host_pad.button&R_CBUTTONS);checks++;
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,0);poll();
    /* If R1 is deliberately rebound to boost, it cannot also toggle camera. */
    configRocketBindings.action[RA_BOOST]=RB_RB;controller_sdl_rocket_bindings_changed();poll();
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);assert(poll().boost&&!(host_pad.button&R_TRIG));checks++;
    car_drawable=0;assert(poll().boost&&!(host_pad.button&R_TRIG));checks++;
    button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);car_drawable=1;rocket_bindings_reset();poll();
    /* A binding held while switching/rebinding only acquires its new camera
     * meaning after release. 5000 is live car throttle below SM64's threshold. */
    for(int low=0;low<2;low++) {
        trigger(low?-22768:32767);assert(poll().throttle>0); // Virtual -22768 maps to about 5000.
        car_selected=0;poll();assert(!(host_pad.button&R_TRIG));checks++;
        trigger(32767);poll();assert(!(host_pad.button&R_TRIG));checks++;
        trigger(-32768);poll();trigger(32767);poll();assert(host_pad.button&R_TRIG);checks++;
        car_selected=1;assert(poll().throttle==0&&!(host_pad.button&R_TRIG));checks++;
        trigger(-32768);poll();trigger(32767);assert(poll().throttle==1&&!(host_pad.button&R_TRIG));checks++;
        trigger(-32768);poll();
    }
    trigger(32767);assert(poll().throttle==1);
    configRocketBindings.action[RA_THROTTLE]=RB_NORTH;controller_sdl_rocket_bindings_changed();
    poll();assert(!(host_pad.button&R_TRIG));checks++;
    trigger(-32768);poll();trigger(32767);poll();assert(host_pad.button&R_TRIG);checks++;
    trigger(-32768);rocket_bindings_reset();poll();
    /* Menus/dialogs regain ordinary confirm after releasing gameplay input. */
    for(int menu=0;menu<3;menu++) {
        button(SDL_CONTROLLER_BUTTON_A,1);assert(poll().jump);checks++;
        if(menu==0)sCurrPlayMode=PLAY_MODE_PAUSED;
        if(menu==1)gDialogID=0;
        if(menu==2)gDjuiInMainMenu=true;
        poll();assert(!(host_pad.button&A_BUTTON));checks++;
        button(SDL_CONTROLLER_BUTTON_A,0);poll();
        button(SDL_CONTROLLER_BUTTON_A,1);poll();assert(host_pad.button&A_BUTTON);checks++;
        sCurrPlayMode=0;gDialogID=DIALOG_NONE;gDjuiInMainMenu=false;
        assert(!poll().jump);checks++;
        button(SDL_CONTROLLER_BUTTON_A,0);poll();
    }
    wheel_enabled=0;
    printf("PASS door input handoff: %u checks; held/released/repeated RT, transient physics ownership, camera controls, remaps and character switches\n",checks);
}
