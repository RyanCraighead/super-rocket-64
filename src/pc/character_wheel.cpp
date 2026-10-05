/* Character wheel. UI never writes actor/controller ownership itself;
 * the game-thread switch manager validates and commits each selection. */
#include "character_wheel.h"
#include "game/character_switch.h"
#include "gfx/wheel_overlay_gl.h"
extern "C" {
#include "controller/controller_keyboard.h"
#include "controller/controller_mouse.h"
}
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "gfx/character_wheel_font.inc"

namespace {
constexpr float pi=3.14159265358979323846f;
constexpr int width=1024,height=768;
constexpr float sector = 2*pi/CHARACTER_COUNT;
bool opened=false,windowActive=true,hotkeyHeld=false,oldRelative=false;
bool gamepadOpened=false,gamepadHeld=false,gamepadRearm=true,gamepadKeyHeld=false,gamepadDispatch=false;
int awaitingSelection=-1;
int hovered=-1,oldCursor=SDL_ENABLE,pointerX=512,pointerY=380;
uint16_t blockedButtons=0;
uint32_t blockedMouse=0;
bool physicalKeys[SDL_NUM_SCANCODES]={},blockedKeys[SDL_NUM_SCANCODES]={};
bool blockedStick=false,blockedCamera=false,closeLatch=false;
uint32_t opens=0,cancels=0;
Uint32 noticeUntil=0;
char notice[256]="Hold F7, point, release to switch";
std::vector<unsigned char> pixels(width*height*4);
struct Color{unsigned char r,g,b,a;};
constexpr Color white={239,246,255,255},muted={174,190,206,255};
const Color colors[CHARACTER_COUNT]={{237,87,91,220},{73,188,119,220},{105,141,246,220},{234,172,72,220},{219,74,96,220},{92,207,215,220},{244,137,59,220}};
void message(const char *s){std::snprintf(notice,sizeof notice,"%s",s?s:"");noticeUntil=SDL_GetTicks()+3000;}
void clearMouse(){mouse_x=mouse_y=0;mouse_buttons=mouse_window_buttons=mouse_window_buttons_pressed=mouse_window_buttons_released=0;mouse_scroll_x=mouse_scroll_y=0;}
void closeWheel(bool canceled){
    if(!opened)return;
    opened=false;gamepadOpened=false;hovered=-1;closeLatch=true;
    if(canceled)++cancels;
    keyboard_on_all_keys_up();clearMouse();
    controller_mouse_leave_relative();
    if(windowActive&&oldRelative&&!0)controller_mouse_enter_relative();
    SDL_ShowCursor(oldCursor);
}
void point(int x,int y){
    SDL_Window *window=SDL_GL_GetCurrentWindow();int w=width,h=height;if(window)SDL_GetWindowSize(window,&w,&h);
    float scale=std::min(w/(float)width,h/(float)height);
    pointerX=scale>0?(int)((x-(w-width*scale)/2)/scale):width/2;
    pointerY=scale>0?(int)((y-(h-height*scale)/2)/scale):380;
    hovered=character_wheel_pick((float)x,(float)y,w,h);
}
void put(int x,int y,Color c){if(x<0||y<0||x>=width||y>=height)return;auto *p=&pixels[(y*width+x)*4];p[0]=c.r;p[1]=c.g;p[2]=c.b;p[3]=c.a;}
void rect(int x,int y,int w,int h,Color c){for(int yy=std::max(y,0);yy<std::min(y+h,height);++yy)for(int xx=std::max(x,0);xx<std::min(x+w,width);++xx)put(xx,yy,c);}
void glyph(int x,int y,unsigned char ch,Color c){
    if(ch<32||ch>126)ch='?';const unsigned char *g=kCharacterWheelFont+(ch-32)*16*26/4;
    for(int yy=0;yy<26;++yy)for(int xx=0;xx<16;++xx){int i=yy*16+xx;unsigned a=((g[i/4]>>(6-2*(i%4)))&3)*85;if(!a)continue;
        int dx=x+xx,dy=y+yy;if(dx<0||dy<0||dx>=width||dy>=height)continue;auto *p=&pixels[(dy*width+dx)*4];
        p[0]=(p[0]*(255-a)+c.r*a)/255;p[1]=(p[1]*(255-a)+c.g*a)/255;p[2]=(p[2]*(255-a)+c.b*a)/255;p[3]=std::max(p[3],(unsigned char)a);}
}
void text(int x,int y,const char *s,Color c){for(;s&&*s&&x<width;s++,x+=16)glyph(x,y,(unsigned char)*s,c);}
void center(int x,int y,const char *s,Color c){text(x-(int)std::strlen(s)*8,y,s,c);}
void compose(){
    std::fill(pixels.begin(),pixels.end(),0);
    if(!opened){
        rect(24,686,976,56,{13,23,35,224});text(40,701,notice,white);return;
    }
    rect(0,0,width,height,{6,12,22,160});
    center(512,24,character_switch_online()?"ONLINE: MARIO / OCTANE":"CHOOSE YOUR CHARACTER",white);
    center(512,57,gamepadOpened?"Hold Share/Back  /  Aim stick  /  Release":"Hold F7  /  Point  /  Release to switch",muted);
    for(int y=92;y<668;++y)for(int x=216;x<808;++x){
        float dx=x-512.f,dy=y-380.f,r=std::sqrt(dx*dx+dy*dy);if(r>280||r<90)continue;
        int slot=character_wheel_pick((float)x,(float)y,width,height);if(slot<0)continue;
        Color c=colors[slot];bool enabled=character_switch_available((CharacterSwitchId)slot);
        if(!enabled)c={49,56,66,238};else if(slot!=hovered){c.r/=3;c.g/=3;c.b/=3;c.a=230;}
        float angle=(float)std::atan2((double)dx,(double)-dy)+sector/2;if(angle<0)angle+=2*pi;
        float edge=std::fmod(angle,sector);if(edge<.017f||edge>sector-.017f)c={16,26,38,248};
        if(r>276||(r<94))c=slot==hovered?white:Color{87,106,122,230};put(x,y,c);
    }
    for(int i=0;i<CHARACTER_COUNT;++i){
        float a=i*(sector);int x=512+(int)(std::sin(a)*213),y=380-(int)(std::cos(a)*213);
        bool avail=character_switch_available((CharacterSwitchId)i);Color c=avail?white:muted;
        bool duo=i==CHARACTER_BANJO;
        const char *label=duo?"Banjo +":character_switch_name((CharacterSwitchId)i);
        int n=(int)std::strlen(label);rect(x-n*8-10,y-20,n*16+20,duo?86:58,{8,17,28,185});
        center(x,y-19,label,c);
        if(duo)center(x,y+9,"Kazooie",c);
        center(x,y+(duo?37:9),!avail?"UNAVAILABLE":(i==(int)character_switch_active()?"CURRENT":""),muted);
    }
    center(512,353,"KEEP",white);center(512,381,"CURRENT",muted);
    center(512,676,gamepadOpened?"Circle / B cancels; center keeps current":"Escape or right-click cancels",muted);
    const char *detail=hovered<0?"Release here to keep your character":
        !character_switch_available((CharacterSwitchId)hovered)?character_switch_reason((CharacterSwitchId)hovered):
        character_switch_name((CharacterSwitchId)hovered);
    char bounded[61];std::snprintf(bounded,sizeof bounded,"%s",detail);center(512,715,bounded,white);
    for(int y=-4;y<=4;++y)for(int x=-4;x<=4;++x)if(x*x+y*y<=16)put(pointerX+x,pointerY+y,white);
}
}
extern "C" int character_wheel_pick(float x,float y,int w,int h){
    if(w<=0||h<=0||!std::isfinite(x)||!std::isfinite(y))return -1;
    float scale=std::min(w/(float)width,h/(float)height);
    float dx=(x-(w-width*scale)/2)/scale-512.f,dy=(y-(h-height*scale)/2)/scale-380.f,r2=dx*dx+dy*dy;
    if(r2<90*90||r2>280*280)return -1;
    float angle=(float)std::atan2((double)dx,(double)-dy)+sector/2;if(angle<0)angle+=2*pi;
    return (int)(angle/(sector))%CHARACTER_COUNT;
}
extern "C" int character_wheel_is_open(void){return opened;}
extern "C" int character_wheel_blocks_gameplay(void){return character_switch_enabled()&&(opened||!windowActive);}
extern "C" int character_wheel_pauses_world(void){return character_wheel_blocks_gameplay()&&!character_switch_online();}
extern "C" int character_wheel_owns_pointer(void){return opened;}
extern "C" void character_wheel_update(void){
    if(!character_switch_enabled())return;
    if(awaitingSelection>=0&&character_switch_pending()<0){
        if(character_switch_active()==awaitingSelection)message(character_switch_name((CharacterSwitchId)awaitingSelection));
        else {const char *reason=character_switch_can_open();message(reason?reason:"Selection canceled");}
        awaitingSelection=-1;
    }
    if(opened){const char *reason=character_switch_can_open();if(reason){message(reason);closeWheel(true);}}
}
extern "C" int character_wheel_handle_event(const SDL_Event *e){
    if(!e||!character_switch_enabled())return 0;
    if(e->type==SDL_WINDOWEVENT){
        if(e->window.event==SDL_WINDOWEVENT_FOCUS_LOST||e->window.event==SDL_WINDOWEVENT_HIDDEN||e->window.event==SDL_WINDOWEVENT_MINIMIZED){
            int count=0;const Uint8 *keys=SDL_GetKeyboardState(&count);
            for(int i=1;i<SDL_NUM_SCANCODES;++i)blockedKeys[i]=blockedKeys[i]||physicalKeys[i]||(keys&&i<count&&keys[i]);
            blockedMouse|=SDL_GetMouseState(nullptr,nullptr);
            character_switch_cancel_pending();awaitingSelection=-1;
            windowActive=false;hotkeyHeld=false;message("Selection canceled: window focus lost");closeWheel(true);keyboard_on_all_keys_up();clearMouse();
        }else if(e->window.event==SDL_WINDOWEVENT_FOCUS_GAINED){windowActive=true;hotkeyHeld=false;keyboard_on_all_keys_up();}
        else if(e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED){character_switch_cancel_pending();awaitingSelection=-1;message("Selection canceled: window resized");closeWheel(true);}
        return 0;
    }
    if(e->type==SDL_QUIT){closeWheel(true);return 0;}
    if((e->type==SDL_KEYDOWN||e->type==SDL_KEYUP)&&e->key.keysym.scancode>SDL_SCANCODE_UNKNOWN&&e->key.keysym.scancode<SDL_NUM_SCANCODES){
        int key=e->key.keysym.scancode;physicalKeys[key]=e->type==SDL_KEYDOWN;
        if(key!=SDL_SCANCODE_F7&&blockedKeys[key]){
            if(e->type==SDL_KEYUP)blockedKeys[key]=false;
            return 1;
        }
    }
    if(e->type==SDL_KEYUP&&e->key.keysym.scancode==SDL_SCANCODE_F7){
        if(gamepadKeyHeld&&!gamepadDispatch)return 1;
        hotkeyHeld=false;
        if(opened){int id=hovered;closeWheel(id<0);
            if(id>=0){if(character_switch_available((CharacterSwitchId)id)&&character_switch_request((CharacterSwitchId)id)){awaitingSelection=id;message("Switching character...");}
                else{++cancels;const char *reason=character_switch_can_open();message(reason?reason:character_switch_reason((CharacterSwitchId)id));}}
        }
        return 1;
    }
    if(e->type==SDL_KEYDOWN&&e->key.keysym.scancode==SDL_SCANCODE_F7){
        if(e->key.repeat||hotkeyHeld)return 1;hotkeyHeld=true;
        if(!windowActive||0)return 0;
        const char *reason=character_switch_can_open();if(reason){message(reason);return 1;}
        if(!wheel_overlay_init()){message("Character wheel renderer unavailable");return 1;}
        awaitingSelection=-1;
        if(!character_switch_prepare_open()){message(character_switch_can_open());return 1;}
        oldRelative=mouse_relative_enabled;oldCursor=SDL_ShowCursor(SDL_QUERY);
        controller_mouse_leave_relative();
        /* Codex can restore an older SDL capture independently of the wrapper
         * flag; force the real SDL mode free after its ownership handoff. */
        SDL_SetRelativeMouseMode(SDL_FALSE);SDL_ShowCursor(SDL_ENABLE);
        opened=true;hovered=-1;++opens;pointerX=512;pointerY=380;
        int count=0;const Uint8 *keys=SDL_GetKeyboardState(&count);
        for(int i=1;i<SDL_NUM_SCANCODES;++i)blockedKeys[i]=physicalKeys[i]||(keys&&i<count&&keys[i]);
        blockedKeys[SDL_SCANCODE_F7]=false;
        blockedMouse|=SDL_GetMouseState(nullptr,nullptr);
        keyboard_on_all_keys_up();clearMouse();
        SDL_Window *w=SDL_GL_GetCurrentWindow();if(w){int ww,hh;SDL_GetWindowSize(w,&ww,&hh);float scale=std::min(ww/(float)width,hh/(float)height);SDL_WarpMouseInWindow(w,ww/2,hh/2-(int)(4*scale));}
        return 1;
    }
    if(!windowActive){
        if(e->type==SDL_KEYDOWN&&e->key.keysym.scancode>0&&e->key.keysym.scancode<SDL_NUM_SCANCODES)blockedKeys[e->key.keysym.scancode]=true;
        return e->type==SDL_KEYDOWN||e->type==SDL_KEYUP||e->type==SDL_TEXTINPUT||e->type==SDL_TEXTEDITING||e->type==SDL_MOUSEMOTION||e->type==SDL_MOUSEBUTTONDOWN||e->type==SDL_MOUSEBUTTONUP||e->type==SDL_MOUSEWHEEL;
    }
    if(!opened)return 0;
    if(e->type==SDL_KEYDOWN&&e->key.keysym.scancode==SDL_SCANCODE_ESCAPE){blockedKeys[SDL_SCANCODE_ESCAPE]=true;message("Selection canceled");closeWheel(true);return 1;}
    if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_RIGHT){blockedMouse|=SDL_BUTTON(SDL_BUTTON_RIGHT);message("Selection canceled");closeWheel(true);return 1;}
    if(e->type==SDL_MOUSEMOTION){point(e->motion.x,e->motion.y);return 1;}
    if(e->type==SDL_KEYDOWN&&e->key.keysym.scancode>0&&e->key.keysym.scancode<SDL_NUM_SCANCODES)blockedKeys[e->key.keysym.scancode]=true;
    if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button>0&&e->button.button<=31)blockedMouse|=SDL_BUTTON(e->button.button);
    return e->type==SDL_KEYDOWN||e->type==SDL_KEYUP||e->type==SDL_TEXTINPUT||e->type==SDL_TEXTEDITING||e->type==SDL_MOUSEBUTTONDOWN||e->type==SDL_MOUSEBUTTONUP||e->type==SDL_MOUSEWHEEL;
}
extern "C" void character_wheel_gamepad(int connected,int allowed,int hold,int cancel,int16_t x,int16_t y){
    if(!character_switch_enabled())return;
    if(!connected||!allowed||!windowActive){
        gamepadRearm=true;gamepadHeld=false;
        if(gamepadOpened){message("Selection canceled");closeWheel(true);}
        if(gamepadKeyHeld){hotkeyHeld=false;gamepadKeyHeld=false;}
        return;
    }
    if(gamepadRearm){if(!hold&&!cancel&&std::abs((int)x)<8000&&std::abs((int)y)<8000)gamepadRearm=false;return;}
    if(hold&&!gamepadHeld&&!opened&&!hotkeyHeld){
        SDL_Event e={};e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_F7;
        gamepadDispatch=true;character_wheel_handle_event(&e);gamepadDispatch=false;gamepadOpened=opened;gamepadKeyHeld=true;
    }
    if(gamepadOpened&&cancel){message("Selection canceled");closeWheel(true);hotkeyHeld=false;gamepadKeyHeld=false;gamepadRearm=true;}
    if(gamepadOpened){
        float radius=std::hypot((float)x,(float)y);
        hovered=-1;pointerX=512;pointerY=380;
        if(radius>12000){
            float angle=std::atan2((double)x,(double)-y)+sector/2;if(angle<0)angle+=2*pi;
            hovered=(int)(angle/(sector))%CHARACTER_COUNT;
            pointerX=512+(int)(213*x/radius);pointerY=380+(int)(213*y/radius);
        }
    }
    if(gamepadKeyHeld&&!hold&&gamepadHeld){
        SDL_Event e={};e.type=SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_F7;gamepadDispatch=true;character_wheel_handle_event(&e);gamepadDispatch=false;
        gamepadOpened=false;gamepadKeyHeld=false;gamepadRearm=true;
    }
    gamepadHeld=hold;
}
extern "C" uint32_t character_wheel_filter_mouse(uint32_t buttons){
    if(!character_switch_enabled())return buttons;
    if(character_wheel_blocks_gameplay()||closeLatch){blockedMouse|=buttons;return 0;}
    blockedMouse&=buttons;return buttons&~blockedMouse;
}
extern "C" void character_wheel_filter_input(uint16_t *buttons,int8_t *x,int8_t *y,int8_t *cx,int8_t *cy){
    if(!character_switch_enabled()||!buttons||!x||!y||!cx||!cy)return;
    if(character_wheel_blocks_gameplay()||closeLatch){
        if(!character_wheel_blocks_gameplay())closeLatch=false;
        blockedButtons|=*buttons;blockedStick|=std::abs((int)*x)>16||std::abs((int)*y)>16;blockedCamera|=std::abs((int)*cx)>16||std::abs((int)*cy)>16;
        *buttons=0;*x=*y=*cx=*cy=0;return;
    }
    blockedButtons&=*buttons;*buttons&=~blockedButtons;
    if(std::abs((int)*x)<=16&&std::abs((int)*y)<=16)blockedStick=false;
    if(std::abs((int)*cx)<=16&&std::abs((int)*cy)<=16)blockedCamera=false;
    if(blockedStick)*x=*y=0;if(blockedCamera)*cx=*cy=0;
}
extern "C" void character_wheel_render(int w,int h){
    if(!character_switch_enabled()||(!opened&&(int32_t)(noticeUntil-SDL_GetTicks())<=0))return;
    compose();if(!wheel_overlay_render(pixels.data(),width,height,w,h)){message("Character wheel renderer unavailable");closeWheel(true);}
}
extern "C" void character_wheel_shutdown(void){character_switch_cancel_pending();awaitingSelection=-1;closeWheel(false);hotkeyHeld=false;gamepadOpened=gamepadHeld=gamepadKeyHeld=false;gamepadRearm=true;blockedButtons=0;blockedMouse=0;std::fill(physicalKeys,physicalKeys+SDL_NUM_SCANCODES,false);std::fill(blockedKeys,blockedKeys+SDL_NUM_SCANCODES,false);blockedStick=blockedCamera=closeLatch=false;windowActive=true;}
extern "C" int character_wheel_snapshot(CharacterWheelSnapshot *out){
    if(!out||!character_switch_enabled())return 0;
    *out={};out->open=opened;out->hovered=hovered;out->window_active=windowActive;out->active=character_switch_active();out->opens=opens;CharacterSwitchSnapshot committed={};if(character_switch_snapshot(&committed))out->commits=committed.commits;out->cancels=cancels;std::snprintf(out->status,sizeof out->status,"%s",notice);return 1;
}
