#include "rocket_tutorial.h"
#include "ingame_menu.h"
#include "level_info.h"
#include "hardcoded.h"
#include "level_update.h"
#include "area.h"
#include "dialog_ids.h"
#include "level_table.h"
#include "character_switch.h"
#include "pc/cliopts.h"
#include "pc/configfile.h"
#include "pc/rocket_bindings.h"
#include "pc/rocket_boost.h"
#include "pc/controller/controller_sdl.h"
#include "pc/controller/controller_bind_mapping.h"
#include <stdio.h>
#include <string.h>

#if defined(VERSION_US) || defined(VERSION_EU)

static const char *pad_name(unsigned binding,int device) {
    static const char *const names[4][RB_COUNT]={
        {"Unbound","A","B","X","Y","LB","RB","Left stick click","Right stick click","D-pad Up","D-pad Down","D-pad Left","D-pad Right","LT","RT"},
        {"Unbound","Cross","Circle","Square","Triangle","L1","R1","L3","R3","D-pad Up","D-pad Down","D-pad Left","D-pad Right","L2","R2"},
        {"Unbound","B","A","Y","X","L","R","Left stick click","Right stick click","D-pad Up","D-pad Down","D-pad Left","D-pad Right","ZL","ZR"},
        {"Unbound","South button","East button","West button","North button","Left bumper","Right bumper","Left stick click","Right stick click","D-pad Up","D-pad Down","D-pad Left","D-pad Right","Left trigger","Right trigger"}
    };
    if(binding>=RB_COUNT)binding=RB_NONE;
    int row=device==CONTROLLER_PROMPT_PLAYSTATION?1:device==CONTROLLER_PROMPT_NINTENDO?2:device==CONTROLLER_PROMPT_XBOX?0:3;
    return names[row][binding];
}
static void keyboard_name(char out[64],const char *name,unsigned key) {
    if(!*name){snprintf(out,64,"Key %04X",key);return;}
    size_t used=0;
    for(const unsigned char *p=(const unsigned char*)name;*p;p++) {
        if(*p<32||*p>126){snprintf(out,64,"Key %04X",key);return;}
        const char *special=NULL;
        switch(*p){case ';':special="Semicolon";break;case '`':special="Grave";break;
            case '.':special="Period";break;case ',':special="Comma";break;case '\'':special="Apostrophe";break;
            case '-':special="Minus";break;case '?':special="Question mark";break;case '$':special="Dollar";break;
            case '%':special="Percent";break;case '#':special="Hash";break;case '@':special="At";break;
            case '[':special="Left bracket";break;case ']':special="Right bracket";break;
            case '\\':special="Backslash";break;case '/':special="Slash";break;case '=':special="Equals";break;
            case '+':special="Plus";break;case '*':special="Asterisk";break;case '^':special="Caret";break;
            case '<':special="Less than";break;case '>':special="Greater than";break;default:break;}
        int written=special?snprintf(out+used,64-used,"%s%s",used?" ":"",special):
            snprintf(out+used,64-used,"%c",*p);
        if(written<0||(size_t)written>=64-used){out[63]=0;return;}used+=(size_t)written;
    }
}
static void native_name(char out[64],const unsigned *keys,int device) {
    snprintf(out,64,"Unbound");
    for(int i=0;i<MAX_BINDS;i++) {
        unsigned key=keys[i];
        if(key==VK_INVALID)continue;
        if(device==CONTROLLER_PROMPT_KEYBOARD) {
            if(key>=VK_BASE_SDL_GAMEPAD&&key<VK_BASE_SDL_MOUSE)continue;
            const char *name=translate_bind_to_name((int)key);
            keyboard_name(out,name,key);
            return;
        }
        if(key<VK_BASE_SDL_GAMEPAD||key>=VK_BASE_SDL_MOUSE)continue;
        static const unsigned native[]={0,1,2,3,9,10,7,8,11,12,13,14,26,27};
        for(unsigned j=0;j<sizeof native/sizeof *native;j++)if(key-VK_BASE_SDL_GAMEPAD==native[j]){
            snprintf(out,64,"%s",pad_name(j+1,device));return;
        }
        if(key==VK_BASE_SDL_GAMEPAD+6)snprintf(out,64,"%s",device==CONTROLLER_PROMPT_PLAYSTATION?"Options or Start":"Start");
        else snprintf(out,64,"Button %u",key-VK_BASE_SDL_GAMEPAD);
        return;
    }
}
static void action_name(char out[64],int action,int device) {
    if(device!=CONTROLLER_PROMPT_KEYBOARD) {
        const RocketBindings *b=rocket_bindings_valid(&configRocketBindings)?&configRocketBindings:&rocket_default_bindings;
        snprintf(out,64,"%s",pad_name(b->action[action],device));return;
    }
    const unsigned *keys=action==RA_THROTTLE?configKeyStickUp:action==RA_BRAKE?configKeyStickDown:
        action==RA_JUMP?configKeyA:action==RA_BOOST?configKeyB:action==RA_CAMERA?configKeyY:configKeyZ;
    native_name(out,keys,device);
}
static unsigned char encode(char c) {
    char input[]={c,0};unsigned char result[2];
    convert_string_ascii_to_sm64(result,input,false);return result[0];
}
static int glyph_width(unsigned char c) {
    return c==DIALOG_CHAR_SLASH?2*gDialogCharWidths[DIALOG_CHAR_SPACE]:gDialogCharWidths[c];
}
/* Explicit six-line pages keep related instructions together. Native glyph
 * widths wrap every current key/button name, including long individual words.
 * Single-character conversion avoids compressed 'the'/'you' width opcodes. */
static int wrap(const char *input,int lines,uint8_t *out) {
    int n=0,line=0,width=0;
#define EMIT(c) do{if(n>=ROCKET_TUTORIAL_CAPACITY-1)return 0;out[n++]=(c);}while(0)
#define NEWLINE() do{EMIT(DIALOG_CHAR_NEWLINE);line++;width=0;}while(0)
    while(*input) {
        if(*input=='\n'||*input=='\f') {
            char separator=*input++;NEWLINE();
            if(separator=='\f')while(line%lines)NEWLINE();
            continue;
        }
        if(*input==' '){input++;continue;}
        const char *end=input;int word=0;
        while(*end&&*end!=' '&&*end!='\n'&&*end!='\f')word+=glyph_width(encode(*end++));
        if(width&&width+gDialogCharWidths[DIALOG_CHAR_SPACE]+word>ROCKET_TUTORIAL_LINE_WIDTH)NEWLINE();
        if(width){EMIT(DIALOG_CHAR_SPACE);width+=gDialogCharWidths[DIALOG_CHAR_SPACE];}
        while(input<end) {
            unsigned char c=encode(*input++);int advance=glyph_width(c);
            if(width+advance>ROCKET_TUTORIAL_LINE_WIDTH)NEWLINE();
            EMIT(c);width+=advance;
        }
    }
    out[n]=DIALOG_CHAR_TERMINATOR;return 1;
#undef NEWLINE
#undef EMIT
}
int rocket_tutorial_page(const uint8_t *text,int position,int lines) {
    int count=0;
    if(!text||lines<1)return 0;
    for(int i=0;i<position&&i<ROCKET_TUTORIAL_CAPACITY&&text[i]!=DIALOG_CHAR_TERMINATOR;i++)count+=text[i]==DIALOG_CHAR_NEWLINE;
    return count/lines;
}
int rocket_tutorial_page_start(const uint8_t *text,int page,int lines) {
    int line=0,start=0;
    if(!text||page<1||lines<1)return 0;
    for(int i=0;i<ROCKET_TUTORIAL_CAPACITY&&text[i]!=DIALOG_CHAR_TERMINATOR;i++)if(text[i]==DIALOG_CHAR_NEWLINE&&++line%lines==0) {
        if(text[i+1]==DIALOG_CHAR_TERMINATOR)break;
        start=i+1;if(line/lines==page)break;
    }
    return start;
}
int rocket_tutorial_text(int dialog,int lines,uint8_t text[ROCKET_TUTORIAL_CAPACITY]) {
    if((!gCLIOpts.rocketCar&&!gCLIOpts.characterNet)||dialog<0||dialog>=DIALOG_COUNT||lines<1||lines>8)return 0;
    if(gCLIOpts.characterWheel && (!character_switch_enabled() || character_switch_active()!=CHARACTER_OCTANE))return 0;
    int welcome=gCurrLevelNum==LEVEL_CASTLE_GROUNDS&&dialog==gBehaviorValues.dialogs.LakituIntroDialog;
    int bob=gCurrLevelNum==LEVEL_BOB&&dialog==DIALOG_000;
    int whomp=gCurrLevelNum==LEVEL_WF&&(dialog==DIALOG_030||dialog==gBehaviorValues.dialogs.KingWhompDialog);
    if(!welcome&&!bob&&!whomp)return 0;
    char action[RA_COUNT][64],steer[144],pause[64],options[64],left[64],right[64];
    int device=controller_sdl_prompt_device();
    for(int i=0;i<RA_COUNT;i++)action_name(action[i],i,device);
    if(device==CONTROLLER_PROMPT_KEYBOARD) {
        native_name(left,configKeyStickLeft,device);native_name(right,configKeyStickRight,device);
        snprintf(steer,sizeof steer,"%s and %s",left,right);native_name(pause,configKeyStart,device);
    } else {
        snprintf(steer,sizeof steer,"%s stick",configRocketBindings.stick==1?"Right":"Left");
        snprintf(pause,sizeof pause,"%s",device==CONTROLLER_PROMPT_PLAYSTATION?"Options or Start":device==CONTROLLER_PROMPT_NINTENDO?"Plus":"Start");
    }
    native_name(options,configKeyR,device);
    char draft[ROCKET_TUTORIAL_CAPACITY];int length;
    if(welcome) length=snprintf(draft,sizeof draft,
        "Welcome to Super Rocket64!\nDrive: %s\nBrake: %s\nSteer: %s\f"
        "Jump: %s\nBoost: %s\nCycle camera: %s\f"
        "Pause: %s\nMenu: %s while paused\nOptions, Controls:\n"
        "%s",
        action[RA_THROTTLE],action[RA_BRAKE],steer,action[RA_JUMP],action[RA_BOOST],action[RA_CAMERA],pause,options,
        device==CONTROLLER_PROMPT_KEYBOARD?"Rebind keys here.\nCamera: Extra binds, Y.":"Car Controller to rebind.");
    else if(bob) length=snprintf(draft,sizeof draft,
        "Land on Goombas from\nabove.\nAt supersonic speed you\ncan also ram Goombas\nand Bob-ombs.\f"
        "Coin only boost:\nYellow, red and blue coins\neach add 5, up to 100.\nBoost: %s\n%s\f"
        "%s\nPowerslide: %s.\nEase off before turns.",action[RA_BOOST],
        rocket_boost_mode()==ROCKET_BOOST_INFINITE?"Mode: Infinite boost.":"Mode: Coin only.",
        rocket_surface_native(rocket_surface_mode())?"Native surfaces:\nIce and slick slopes\nreduce grip.":"Octane surfaces:\nIce uses normal car grip.",action[RA_SLIDE]);
    else length=snprintf(draft,sizeof draft,
        "Let a Whomp fall flat.\nGet on its exposed back.\nRest all four wheels on\nits back to beat a small\nWhomp.\f"
        "King Whomp takes one hit\nper fall. Let him fall\nagain, then hit the back.\nRepeat until he is beaten.\f"
        "Hit the exposed back\nwith a flip or a fast\nnose-down boosted dive.\nBoost: %s.",action[RA_BOOST]);
    if(length<0||length>=(int)sizeof draft)return 0;
    return wrap(draft,lines,text);
}
#else
int rocket_tutorial_text(int dialog,int lines,uint8_t text[ROCKET_TUTORIAL_CAPACITY]) {
    (void)dialog;(void)lines;(void)text;return 0;
}
int rocket_tutorial_page(const uint8_t *text,int position,int lines) {
    (void)text;(void)position;(void)lines;return 0;
}
int rocket_tutorial_page_start(const uint8_t *text,int page,int lines) {
    (void)text;(void)page;(void)lines;return 0;
}
#endif
