static void test_text_gamepad(void) {
    unsigned checks=0;wheel_enabled=1;
    const int bindings[]={RB_SOUTH,RB_RB,RB_RT};
    const SDL_GameControllerButton buttons[]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_A};
    for(int i=0;i<3;i++) {
        rocket_bindings_reset();configRocketBindings.action[RA_JUMP]=bindings[i];
        controller_sdl_rocket_bindings_changed();poll();
        if(i==2)trigger(32767);else button(buttons[i],1);
        assert(poll().jump&&!(host_pad.button&A_BUTTON));checks++; // native isolation
        gDialogID=0;car_drawable=0;
        for(int held=0;held<5;held++){poll();assert(!(host_pad.button&A_BUTTON));checks++;}
        if(i==2)trigger(-32768);else button(buttons[i],0);
        poll();
        for(int page=0;page<3;page++) {
            if(i==2)trigger(32767);else button(buttons[i],1);
            poll();assert((host_pad.button&A_BUTTON)&&!(host_pad.button&R_TRIG));checks++;
            if(page<2){if(i==2)trigger(-32768);else button(buttons[i],0);poll();assert(!(host_pad.button&A_BUTTON));checks++;}
        }
        gDialogID=DIALOG_NONE;car_drawable=1;
        for(int held=0;held<5;held++){assert(!poll().jump&&!(host_pad.button&A_BUTTON));checks++;}
        if(i==2)trigger(-32768);else button(buttons[i],0);poll();
        if(i==2)trigger(32767);else button(buttons[i],1);
        assert(poll().jump&&!(host_pad.button&A_BUTTON));checks++;
        if(i==2)trigger(-32768);else button(buttons[i],0);poll();
    }
    rocket_bindings_reset();wheel_enabled=0;poll();
    printf("PASS mapped dialog controller: %u checks; Cross/R1/R2 jump, native isolation, fresh page confirmation and held close/reopen prevention\n",checks);
}
