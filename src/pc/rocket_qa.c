/* Opt-in native QA: reads the real game and framebuffer, submits ordinary SDL
 * events to this executable only. Never edits player/physics state or uses OS
 * input. No effect without ROCKET_CAR_QA=1 and SM64_ROCKET_QA_OUTPUT. */
#ifdef ROCKET_CAR_QA
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sm64.h"
#include "dialog_ids.h"
#include "level_table.h"
#include "object_fields.h"
#include "game/level_update.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/interaction.h"
#include "game/object_list_processor.h"
#include "game/display.h"
#include "cliopts.h"
#include "rocket_runtime.h"
#include "djui/djui_console.h"
#include "network/network.h"

static SDL_Joystick *qaPad;
static int qaPadIndex=-1;
static unsigned savedGamepadNumber;
static void gamepad_select(void){
    if(!qaPad||!SDL_JoystickGetAttached(qaPad))return;
    SDL_JoystickID instance=SDL_JoystickInstanceID(qaPad);
    for(int index=0;index<SDL_NumJoysticks();++index)if(SDL_JoystickGetDeviceInstanceID(index)==instance){
        if(configGamepadNumber!=(unsigned)index)fprintf(stderr,"ROCKET_QA_GAMEPAD_SELECT old=%u new=%d instance=%d\n",configGamepadNumber,index,instance);
        configGamepadNumber=(unsigned)index;qaPadIndex=index;return;
    }
}
static void gamepad_attach(void){
    qaPadIndex=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);
    if(qaPadIndex<0||!SDL_IsGameController(qaPadIndex))exit(87);
    qaPad=SDL_JoystickOpen(qaPadIndex);if(!qaPad)exit(87);
    SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
    SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
    configGamepadNumber=(unsigned)qaPadIndex;
    fprintf(stderr,"ROCKET_QA_GAMEPAD standardized SDL virtual controller index=%d\n",qaPadIndex);
}
static void gamepad_begin(void){
    if(!getenv("SM64_ROCKET_QA_GAMEPAD"))return;
    savedGamepadNumber=configGamepadNumber;gamepad_attach();
}
static void key(SDL_Window *window,SDL_Scancode code,int down){
    SDL_Event event={0};event.type=down?SDL_KEYDOWN:SDL_KEYUP;event.key.windowID=SDL_GetWindowID(window);
    event.key.state=down?SDL_PRESSED:SDL_RELEASED;event.key.keysym.scancode=code;event.key.keysym.sym=SDL_GetKeyFromScancode(code);
    if(SDL_PushEvent(&event)!=1)exit(81);
}
static void capture(SDL_Window *window,const char *directory,unsigned elapsed){
    int width,height;SDL_GL_GetDrawableSize(window,&width,&height);if(width<1||height<1||width>4096||height>4096)exit(82);
    unsigned char *pixels=malloc((size_t)width*height*3);if(!pixels)exit(83);
    GLint alignment;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels);glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    char path[4096];snprintf(path,sizeof(path),"%s/frame-%05u.ppm",directory,elapsed);FILE *file=fopen(path,"wb");if(!file)exit(84);
    fprintf(file,"P6\n%d %d\n255\n",width,height);for(int y=height-1;y>=0;--y)fwrite(pixels+(size_t)y*width*3,1,(size_t)width*3,file);fclose(file);free(pixels);
    fprintf(stderr,"ROCKET_QA_CAPTURE %s renderer=%s\n",path,glGetString(GL_RENDERER));
}
struct Event {unsigned time;SDL_Scancode key;int down;};
static void input_event(SDL_Window *window,const struct Event *event){
    if(qaPad){
        int button=event->key==SDL_SCANCODE_L?SDL_CONTROLLER_BUTTON_A:
            event->key==SDL_SCANCODE_COMMA?SDL_CONTROLLER_BUTTON_B:
            event->key==SDL_SCANCODE_K?SDL_CONTROLLER_BUTTON_X:
            event->key==SDL_SCANCODE_SPACE?SDL_CONTROLLER_BUTTON_START:-1;
        if(button>=0){SDL_JoystickSetVirtualButton(qaPad,button,event->down);return;}
        if(event->key==SDL_SCANCODE_W&&event->time>=8000){SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,event->down?32767:-32768);return;}
        if(event->key==SDL_SCANCODE_W||event->key==SDL_SCANCODE_S){SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_LEFTY,event->down?(event->key==SDL_SCANCODE_W?-32768:32767):0);return;}
        if(event->key==SDL_SCANCODE_D){SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_LEFTX,event->down?32767:0);return;}
    }
    key(window,event->key,event->down);
}
static const struct Event events[]={
    {1000,SDL_SCANCODE_L,1},{1200,SDL_SCANCODE_L,0},{1300,SDL_SCANCODE_L,1},{1400,SDL_SCANCODE_L,0},
    {4000,SDL_SCANCODE_L,1},{4100,SDL_SCANCODE_L,0},{4100,SDL_SCANCODE_W,1},{4250,SDL_SCANCODE_L,1},
    {4400,SDL_SCANCODE_L,0},{4400,SDL_SCANCODE_W,0},{4600,SDL_SCANCODE_S,1},{4800,SDL_SCANCODE_S,0},
    {6000,SDL_SCANCODE_SPACE,1},{6080,SDL_SCANCODE_SPACE,0},{6700,SDL_SCANCODE_L,1},{6700,SDL_SCANCODE_COMMA,1},
    {7500,SDL_SCANCODE_L,0},{7500,SDL_SCANCODE_COMMA,0},
    {8000,SDL_SCANCODE_W,1},{8000,SDL_SCANCODE_COMMA,1},{8400,SDL_SCANCODE_D,1},
    {8600,SDL_SCANCODE_W,0},{8600,SDL_SCANCODE_COMMA,0},{8600,SDL_SCANCODE_D,0},
    {9000,SDL_SCANCODE_F8,1},{9150,SDL_SCANCODE_F8,0},{9300,SDL_SCANCODE_W,1},{9300,SDL_SCANCODE_L,1},{9300,SDL_SCANCODE_COMMA,1},
    {9700,SDL_SCANCODE_W,0},{9700,SDL_SCANCODE_L,0},{9700,SDL_SCANCODE_COMMA,0},{9800,SDL_SCANCODE_ESCAPE,1},{10050,SDL_SCANCODE_ESCAPE,0},
    {10400,SDL_SCANCODE_W,1},{10400,SDL_SCANCODE_D,1},{10400,SDL_SCANCODE_K,1},
    {11100,SDL_SCANCODE_W,0},{11100,SDL_SCANCODE_D,0},{11100,SDL_SCANCODE_K,0}
};
static const struct Event hostEvents[]={
    {1000,SDL_SCANCODE_W,1},{1500,SDL_SCANCODE_W,0},{2000,SDL_SCANCODE_L,1},{2100,SDL_SCANCODE_L,0},
    {3000,SDL_SCANCODE_SPACE,1},{3100,SDL_SCANCODE_SPACE,0},{4000,SDL_SCANCODE_SPACE,1},{4100,SDL_SCANCODE_SPACE,0},
    {4700,SDL_SCANCODE_F8,1},{4800,SDL_SCANCODE_F8,0},{5000,SDL_SCANCODE_W,1},{5300,SDL_SCANCODE_W,0},
    {5400,SDL_SCANCODE_ESCAPE,1},{5500,SDL_SCANCODE_ESCAPE,0}
};
static void input_interrupt_test(SDL_Window *window,const char *directory,unsigned elapsed,const RocketSnapshot *state){
    static int phase[8];static unsigned logged;
    if(!phase[0]&&elapsed>=500){key(window,SDL_SCANCODE_GRAVE,1);key(window,SDL_SCANCODE_GRAVE,0);phase[0]=1;}
    if(!phase[1]&&elapsed>=750){SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_A,1);SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_B,1);phase[1]=1;}
    if(!phase[2]&&elapsed>=2000){key(window,SDL_SCANCODE_ESCAPE,1);key(window,SDL_SCANCODE_ESCAPE,0);capture(window,directory,elapsed);phase[2]=1;}
    if(!phase[3]&&elapsed>=3000){SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_A,0);SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_B,0);phase[3]=1;}
    if(!phase[4]&&elapsed>=4000){SDL_JoystickDetachVirtual(qaPadIndex);SDL_JoystickClose(qaPad);qaPad=NULL;fprintf(stderr,"ROCKET_INPUT_QA_DETACH\n");phase[4]=1;}
    if(!phase[5]&&elapsed>=4500){gamepad_attach();SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_A,1);SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_B,1);fprintf(stderr,"ROCKET_INPUT_QA_RECONNECT_HELD\n");phase[5]=1;}
    if(!phase[6]&&elapsed>=6000){SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_A,0);SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_B,0);capture(window,directory,elapsed);phase[6]=1;}
    if(phase[6]){
        SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_A,elapsed>=6300&&elapsed<6400);
        SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_B,elapsed>=6500&&elapsed<6700);
    }
    if(elapsed>=logged+50){logged=elapsed;RocketInput mapped={0};rocket_runtime_last_input(&mapped);
        fprintf(stderr,"ROCKET_INPUT_QA_STATE {\"ms\":%u,\"focus\":%d,\"console\":%d,\"ticks\":%llu,\"height\":%.4f,\"jump\":%d,\"boost\":%.4f,\"requested_jump\":%d,\"requested_boost\":%d,\"raw_jump\":%d,\"raw_boost\":%d}\n",elapsed,SDL_GetKeyboardFocus()==window,gDjuiConsoleFocus,(unsigned long long)state->ticks,state->position[1],state->jumped,state->boost,mapped.jump,mapped.boost,qaPad?SDL_JoystickGetButton(qaPad,SDL_CONTROLLER_BUTTON_A):0,qaPad?SDL_JoystickGetButton(qaPad,SDL_CONTROLLER_BUTTON_B):0);
    }
    if(elapsed>=8000){configGamepadNumber=savedGamepadNumber;fprintf(stderr,"ROCKET_INPUT_QA_END\n");SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);}
}
static void door_test(SDL_Window *window,const char *directory,unsigned elapsed,const RocketSnapshot *state,int active){
    static unsigned logged,entered,resumed;static int waypoint,transition,captures[4];
    static const float route[][2]={{0,3300},{0,1800},{0,0},{0,-1450}};
    struct MarioState *m=&gMarioStates[0];
    if(!qaPad)exit(89);
    float throttle=0,steer=0,brake=0;
    if(gCurrLevelNum==LEVEL_CASTLE_GROUNDS&&active){
        float tx,tz;
        if(waypoint<4){tx=route[waypoint][0];tz=route[waypoint][1];if(hypotf(tx-m->pos[0],tz-m->pos[2])<350)++waypoint;}
        if(waypoint<4){tx=route[waypoint][0];tz=route[waypoint][1];}
        else {
            tx=-150;tz=-2000;
            struct ObjectNode *head=&gObjectLists[OBJ_LIST_SURFACE];
            for(struct ObjectNode *node=head->next;node&&node!=head;node=node->next){
                struct Object *o=(struct Object*)node;
                if(o->oInteractType==INTERACT_WARP_DOOR&&o->oPosY>700&&o->oPosX<0&&o->oPosX>-500){tx=o->oPosX;tz=o->oPosZ;break;}
            }
        }
        float desired=(float)atan2((double)(tx-m->pos[0]),(double)(tz-m->pos[2]));
        float heading=(float)atan2((double)state->basis[0],(double)state->basis[2]);
        float error=(float)atan2(sin(desired-heading),cos(desired-heading));
        steer=fmaxf(-1,fminf(1,-error*1.5f));
        float speed=hypotf(state->velocity[0],state->velocity[2]);
        throttle=.4f;if(speed>(fabsf(error)>.6f?300.f:500.f)){throttle=0;brake=.25f;}
    }
    if(m->action==ACT_PUSHING_DOOR||m->action==ACT_PULLING_DOOR){
        if(!transition)capture(window,directory,elapsed);
        transition=1;
    }
    if(gCurrLevelNum==LEVEL_CASTLE&&!entered){entered=elapsed;capture(window,directory,elapsed);}
    if(gCurrLevelNum==LEVEL_CASTLE&&active){
        if(!resumed)resumed=elapsed;
        // Leave the native doorway using ordinary input before the final
        // image. Its first resumed frame still has the camera behind the door.
        double desired=atan2((double)(-1100.f-m->pos[0]),(double)(800.f-m->pos[2]));
        double heading=atan2((double)state->basis[0],(double)state->basis[2]);
        float error=(float)atan2(sin(desired-heading),cos(desired-heading));
        steer=fmaxf(-1.f,fminf(1.f,-error*1.5f));
        float speed=hypotf(state->velocity[0],state->velocity[2]);
        throttle=speed<400.f?.4f:0;brake=speed>480.f?.25f:0;
    }
    SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_LEFTX,(Sint16)(steer*32767));
    SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,(Sint16)(-32768+(throttle>0?.1f+.9f*throttle:0)*65535));
    SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_TRIGGERLEFT,(Sint16)(-32768+(brake>0?.1f+.9f*brake:0)*65535));
    // Dismiss the normal first-entry dialog using a face button, never player-state writes.
    SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_A,gCurrLevelNum==LEVEL_CASTLE&&get_dialog_id()!=DIALOG_NONE&&elapsed%700<100);
    if(elapsed>=logged+100){logged=elapsed;fprintf(stderr,"ROCKET_DOOR_QA_STATE {\"ms\":%u,\"level\":%d,\"active\":%d,\"action\":%u,\"waypoint\":%d,\"pos\":[%.3f,%.3f,%.3f],\"steer\":%.3f,\"throttle\":%.3f,\"focus\":%d}\n",elapsed,gCurrLevelNum,active,m->action,waypoint,m->pos[0],m->pos[1],m->pos[2],steer,throttle,SDL_GetKeyboardFocus()==window);}
    const unsigned times[]={3000,8000,13000,20000};for(int i=0;i<4;++i)if(!captures[i]&&elapsed>=times[i]){capture(window,directory,elapsed);captures[i]=1;}
    if(resumed&&active&&elapsed>resumed+2200&&m->pos[2]<1250.f){capture(window,directory,elapsed);fprintf(stderr,"ROCKET_DOOR_QA_END transition=%d entered=1 car_resumed=1\n",transition);configGamepadNumber=savedGamepadNumber;SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);}
    else if(elapsed>35000){capture(window,directory,elapsed);fprintf(stderr,"ROCKET_DOOR_QA_TIMEOUT transition=%d entered=%u\n",transition,entered);exit(90);}
}
#include "rocket_boss_qa.inc.h"
#include "rocket_boss_mario_qa.inc.h"
#include "rocket_combined_qa.inc.h"
#include "rocket_cap_progress_qa.inc.h"
#include "rocket_platform_qa.inc.h"
void rocket_qa_swap(void *pointer){
    static Uint32 start,boot;static size_t next;static int begun,captured[5];static unsigned logged;
    SDL_Window *window=pointer;const char *directory=getenv("SM64_ROCKET_QA_OUTPUT");if(!directory||!*directory)return;
    int hostMode=!gCLIOpts.rocketCar&&getenv("SM64_ROCKET_QA_HOST")!=NULL;
    int networkMotion=gCLIOpts.characterNet&&gCLIOpts.loopbackOnly&&getenv("SM64_CHARACTER_NET_MOTION");
    int marioBoss=getenv("SM64_ROCKET_QA_MARIO_BOSS")!=NULL;
    if(marioBoss&&!boss_mario_qa_admitted())exit(93);
    const char *bossScenario=getenv("SM64_ROCKET_QA_BOSS");
    if(bossScenario&&((!gCLIOpts.offline&&!networkMotion)||!gCLIOpts.rocketCar||
       !getenv("SM64_ROCKET_QA_GAMEPAD")||(strcmp(bossScenario,"bob")&&strcmp(bossScenario,"bob-recovery")&&strcmp(bossScenario,"bowser"))))exit(91);
    if((!gCLIOpts.offline&&!networkMotion)||(!gCLIOpts.rocketCar&&!hostMode&&!marioBoss)||configMasterVolume!=0)exit(85);
    if(!boot){boot=SDL_GetTicks();gamepad_begin();SDL_SetWindowTitle(window,marioBoss?"Mario AUTOMATED BOSS TEST - scripted input - muted":"Octane AUTOMATED TEST - scripted input - muted");SDL_ShowWindow(window);SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);}
    gamepad_select();
    RocketSnapshot state={0};int active=rocket_runtime_snapshot(&state);
    struct MarioState *m=&gMarioStates[0];
    if(marioBoss){
        int ready=0;
        for(int i=1;i<MAX_PLAYERS;i++)if(gNetworkPlayers[i].connected&&gNetworkPlayers[i].currPositionValid)ready=1;
        if(!begun){
            if(ready&&m->marioObj&&m->controller&&m->area&&m->action&&SDL_GetKeyboardFocus()==window){begun=1;start=SDL_GetTicks();}
            else if(SDL_GetTicks()-boot>30000)exit(86);
            else return;
        }
        boss_mario_test(window,directory,SDL_GetTicks()-start);return;
    }
    if(hostMode){
        if(!m->marioObj||!m->controller||!m->area||!m->action){
            if(SDL_GetTicks()-boot>30000){fprintf(stderr,"HOST_QA_TIMEOUT no initialized character\n");exit(86);}return;
        }
        const char *name=gCLIOpts.ootLink?"link":gCLIOpts.bm64Bomberman?"bomberman":gCLIOpts.bkDuo?"banjo":gCLIOpts.spidermanOriginal?"spiderman":"mario";
        if(!begun){begun=1;start=SDL_GetTicks();fprintf(stderr,"HOST_QA_BEGIN %s master_volume=%u\n",name,configMasterVolume);}
        unsigned elapsed=SDL_GetTicks()-start;
        while(next<sizeof(hostEvents)/sizeof(hostEvents[0])&&hostEvents[next].time<=elapsed){key(window,hostEvents[next].key,hostEvents[next].down);++next;}
        if(elapsed>=logged+50){logged=elapsed;fprintf(stderr,"HOST_QA_STATE {\"ms\":%u,\"timer\":%u,\"character\":\"%s\",\"pos\":[%.4f,%.4f,%.4f],\"vel\":[%.4f,%.4f,%.4f],\"action\":%u,\"hidden\":%d,\"health\":%d,\"paused\":%d,\"panel\":%d}\n",elapsed,gGlobalTimer,name,m->pos[0],m->pos[1],m->pos[2],m->vel[0],m->vel[1],m->vel[2],m->action,!!(m->marioObj->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE),m->health,sCurrPlayMode==PLAY_MODE_PAUSED,0);}
        const unsigned times[]={800,1800,2300,3500,5500};for(int i=0;i<5;++i)if(!captured[i]&&elapsed>=times[i]){capture(window,directory,elapsed);captured[i]=1;}
        if(elapsed>=6000){fprintf(stderr,"HOST_QA_END %s\n",name);SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);}return;
    }
    int peerReady=!networkMotion;
    if(networkMotion)for(int i=1;i<MAX_PLAYERS;i++)if(gNetworkPlayers[i].connected&&gNetworkPlayers[i].currPositionValid)peerReady=1;
    if(!begun){if(active&&state.grounded&&peerReady&&SDL_GetKeyboardFocus()==window){begun=1;start=SDL_GetTicks();fprintf(stderr,"ROCKET_QA_BEGIN\n");}else if(SDL_GetTicks()-boot>30000){fprintf(stderr,"ROCKET_QA_TIMEOUT no grounded/focused car or ready peer\n");exit(86);}else return;}
    unsigned elapsed=SDL_GetTicks()-start;
    if(getenv("SM64_ROCKET_QA_CAP_PROGRESS")){cap_progress_test(window,directory,elapsed,&state,active);return;}
    if(getenv("SM64_ROCKET_QA_COMBINED")){combined_test(window,directory,elapsed,&state,active);return;}
    if(getenv("SM64_ROCKET_QA_WORLD")){world_qa_test(window,directory,elapsed,&state,active);return;}
    if(getenv("SM64_ROCKET_QA_BOSS")){boss_test(window,directory,elapsed,&state,active);return;}
    if(getenv("SM64_ROCKET_QA_DOOR")){door_test(window,directory,elapsed,&state,active);return;}
    if(getenv("SM64_ROCKET_QA_INTERRUPTS")){if(!qaPad&&elapsed<4000)exit(88);input_interrupt_test(window,directory,elapsed,&state);return;}
    while(next<sizeof(events)/sizeof(events[0])&&events[next].time<=elapsed){input_event(window,&events[next]);fprintf(stderr,"ROCKET_QA_INPUT %u %d %d\n",elapsed,events[next].key,events[next].down);++next;}
    if(qaPad){
        // Brake on the castle lawn before the native grabbable trees. Those
        // interactions hand ownership back to the host and are a separate gap.
        if(elapsed>=8350&&elapsed<8600){SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);SDL_JoystickSetVirtualButton(qaPad,SDL_CONTROLLER_BUTTON_B,0);}
        SDL_JoystickSetVirtualAxis(qaPad,SDL_CONTROLLER_AXIS_TRIGGERLEFT,(elapsed>=8400&&elapsed<8800)||(elapsed>=11200&&elapsed<11500)?32767:-32768);
    }
    if(elapsed>=logged+50){
        logged=elapsed;
        RocketInput mapped={0};rocket_runtime_last_input(&mapped);
        if(qaPad&&elapsed>=1000&&elapsed<1200)fprintf(stderr,"ROCKET_QA_DEVICE configured=%u instance=%d axes=%d,%d trigger=%d jump=%d controller_init=%u\n",configGamepadNumber,SDL_JoystickInstanceID(qaPad),SDL_JoystickGetAxis(qaPad,0),SDL_JoystickGetAxis(qaPad,1),SDL_JoystickGetAxis(qaPad,5),SDL_JoystickGetButton(qaPad,0),SDL_WasInit(SDL_INIT_GAMECONTROLLER));
        fprintf(stderr,"ROCKET_QA_MAPPED {\"ms\":%u,\"throttle\":%.4f,\"steer\":%.4f,\"pitch\":%.4f,\"yaw\":%.4f,\"roll\":%.4f,\"jump\":%d,\"boost\":%d,\"slide\":%d}\n",elapsed,mapped.throttle,mapped.steer,mapped.pitch,mapped.yaw,mapped.roll,mapped.jump,mapped.boost,mapped.powerslide);
        fprintf(stderr,"ROCKET_QA_STATE {\"ms\":%u,\"timer\":%u,\"active\":%d,\"ticks\":%llu,\"paused\":%d,\"panel\":%d,\"focus\":%d,\"action\":%u,\"pos\":[%.4f,%.4f,%.4f],\"vel\":[%.4f,%.4f,%.4f],\"up\":[%.4f,%.4f,%.4f],\"ground\":%d,\"jump\":%d,\"double\":%d,\"flip\":%d,\"boost\":%.4f,\"stick\":[%d,%d],\"buttons\":%u}\n",
          elapsed,gGlobalTimer,active,(unsigned long long)state.ticks,sCurrPlayMode==PLAY_MODE_PAUSED,0,SDL_GetKeyboardFocus()==window,m->action,
          state.position[0],state.position[1],state.position[2],state.velocity[0],state.velocity[1],state.velocity[2],state.basis[6],state.basis[7],state.basis[8],state.grounded,state.jumped,state.double_jumped,state.flipped,state.boost,m->controller->rawStickX,m->controller->rawStickY,m->controller->buttonDown);
    }
    const unsigned times[]={800,2200,4500,6800,11600};for(int i=0;i<5;++i)if(!captured[i]&&elapsed>=times[i]){capture(window,directory,elapsed);captured[i]=1;}
    if(elapsed>=12000){if(qaPad)configGamepadNumber=savedGamepadNumber;fprintf(stderr,"ROCKET_QA_END\n");SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);}
}
#endif
