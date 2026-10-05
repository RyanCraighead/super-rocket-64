#include "djui.h"
#include "djui_panel.h"
#include "djui_panel_menu.h"
#include "djui_panel_rocket_controls.h"
#include "pc/configfile.h"
#include "pc/controller/controller_sdl.h"
#include "pc/rocket_bindings.h"
#include <stdio.h>
#include <string.h>

static void bindings_changed(UNUSED struct DjuiBase *caller) {
    controller_sdl_rocket_bindings_changed();
    configfile_save(configfile_name());
}
static struct DjuiText *note(struct DjuiBase *body, const char *message, float height) {
    struct DjuiText *text=djui_text_create(body,message);
    djui_base_set_size_type(&text->base,DJUI_SVT_RELATIVE,DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&text->base,1,height);
    djui_text_set_font_scale(text,text->font->defaultFontScale*.75f);
    return text;
}
static void controller_status(struct DjuiBase *base, UNUSED bool *unused) {
    const char *message;
    int count=SDL_NumJoysticks();
    if(configDisableGamepads) message="Gamepads disabled. Bindings stay saved.";
    else if(count<=0 || configGamepadNumber >= (unsigned)count) message="No selected controller. Keyboard still works.\nBindings stay saved for reconnection.";
    else if(!SDL_IsGameController(configGamepadNumber)) message="Selected device needs an SDL controller mapping.\nCar bindings stay saved; keyboard still works.";
    else message="Bindings apply only to this PC's car controller.\nStart pauses; Back/Share opens the character wheel.";
    struct DjuiText *text=(struct DjuiText *)base;
    if(strcmp(text->message,message)) djui_text_set_text(text,message);
}
static void conflicts_status(struct DjuiBase *base, UNUSED bool *unused) {
    char message[512]="Shared actions: ";
    unsigned int conflicts=rocket_bindings_conflicts(&configRocketBindings);
    int first=1;
    for(int i=0;i<RA_COUNT;++i) if(conflicts&(1u<<i)) {
        size_t used=strlen(message);
        snprintf(message+used,sizeof(message)-used,"%s%s",first?"":", ",rocket_action_names[i]);
        first=0;
    }
    if(first) snprintf(message,sizeof(message),"No shared action buttons.");
    else strncat(message,". Shared actions run together; opposite directions cancel.",sizeof(message)-strlen(message)-1);
    struct DjuiText *text=(struct DjuiText *)base;
    if(strcmp(text->message,message)) djui_text_set_text(text,message);
}
static void action_panel(struct DjuiBase *caller, int begin, int end, char *title) {
    struct DjuiThreePanel *panel=djui_panel_menu_create(title,false);
    struct DjuiBase *body=djui_three_panel_get_body(panel);
    char *choices[RB_COUNT];
    for(int i=0;i<RB_COUNT;++i) choices[i]=(char *)rocket_binding_names[i];
    for(int i=begin;i<end;++i)
        djui_selectionbox_create(body,rocket_action_names[i],choices,RB_COUNT,&configRocketBindings.action[i],bindings_changed);
    note(body,"Shared bindings are allowed (including powerslide + air roll).\nUnbound disables this controller action only.",48);
    note(body,"",112)->base.on_render_pre=conflicts_status;
    djui_button_create(body,DLANG(MENU,BACK),DJUI_BUTTON_STYLE_BACK,djui_panel_menu_back);
    djui_panel_add(caller,panel,NULL);
}
static void driving(struct DjuiBase *caller) { action_panel(caller,RA_THROTTLE,RA_SLIDE,"Car Driving"); }
static void air_controls(struct DjuiBase *caller) { action_panel(caller,RA_SLIDE,RA_COUNT,"Car Air / Slide"); }
static void reset_bindings(struct DjuiBase *caller) {
    rocket_bindings_reset();
    bindings_changed(caller);
    /* Refresh only the three axis selectionboxes in this panel. */
    for(struct DjuiBaseChild *child=caller->parent->child;child;child=child->next)
        if(child->base->bTag) djui_selectionbox_update_value(child->base);
}
void djui_panel_rocket_controls_create(struct DjuiBase *caller) {
    struct DjuiThreePanel *panel=djui_panel_menu_create("Car Controller",false);
    struct DjuiBase *body=djui_three_panel_get_body(panel);
    if(!rocket_bindings_valid(&configRocketBindings)) rocket_bindings_reset();
    note(body,"",48)->base.on_render_pre=controller_status;
    djui_button_create(body,"Driving bindings",DJUI_BUTTON_STYLE_NORMAL,driving);
    djui_button_create(body,"Air / slide bindings",DJUI_BUTTON_STYLE_NORMAL,air_controls);
    char *sticks[]={"Left stick","Right stick"};
    char *directions[]={"Normal","Inverted"};
    djui_selectionbox_create(body,"Steering / Pitch",sticks,2,&configRocketBindings.stick,bindings_changed)->base.bTag=true;
    djui_selectionbox_create(body,"Steering direction",directions,2,&configRocketBindings.invert_x,bindings_changed)->base.bTag=true;
    djui_selectionbox_create(body,"Pitch direction",directions,2,&configRocketBindings.invert_y,bindings_changed)->base.bTag=true;
    djui_button_create(body,"Reset car bindings",DJUI_BUTTON_STYLE_NORMAL,reset_bindings);
    djui_button_create(body,DLANG(MENU,BACK),DJUI_BUTTON_STYLE_BACK,djui_panel_menu_back);
    djui_panel_add(caller,panel,NULL);
}
