/* Octane chase mode uses the existing free-camera controls, collision queries
 * and apply path. It owns only the local camera while actual car physics owns
 * the local player. Native doors/cutscenes continue through the normal path. */
#include "pc/rocket_boost.h"
#include "pc/configfile.h"
#include "pc/controller/controller_sdl.h"
static struct RocketChaseCamera {
    int valid;
    struct Area *area;
    u32 epoch,frame;
    s16 level,heading;
    int lookHold;
    float anchor[3],distance,clearDistance;
} rocketChase;
static int rocket_camera_selected(void){return configRocketCameraMode==1&&rocket_adapter_car_selected();}
static void rocket_camera_sync(void){
    static int selected=-1;
    static int cycleHeld=1;
    struct MarioState *m=&gMarioStates[0];
    int allowed=rocket_adapter_car_selected()&&rocket_runtime_owns_controls()&&
        gMarioState==m&&m->area&&m->area->camera&&!m->area->camera->cutscene&&
        m->health>=0x100&&!m->freeze&&sCurrPlayMode==PLAY_MODE_NORMAL&&
        gPlayer1Controller&&!controller_sdl_rocket_input_blocked();
    int down=gPlayer1Controller&&!!(gPlayer1Controller->buttonDown&Y_BUTTON);
    if(!allowed)cycleHeld=1;
    else {
        if(down&&!cycleHeld){
            configRocketCameraMode=configRocketCameraMode==1?0:1;
            configfile_save(configfile_name());
        }
        cycleHeld=down;
    }
    int next=rocket_camera_selected();
    if(next!=selected){rocketChase.valid=0;selected=next;newcam_init_settings();}
    newcam_toggle(camera_config_is_free_cam_enabled()||next||gDjuiInMainMenu);
}
static s16 rocket_camera_heading(const RocketSnapshot *car,s16 previous){
    float length=hypotf(car->basis[0],car->basis[2]);
    if(car->flipping||car->basis[7]<.5f||length<.3f)return previous;
    float x=car->basis[0]/length,z=car->basis[2]/length;
    float speed=hypotf(car->velocity[0],car->velocity[2]);
    /* Looking behind a reversing car must not turn through 180 degrees. */
    if(speed>200.f*rocket_speed_scale()&&(car->velocity[0]*x+car->velocity[2]*z)>speed*.25f){
        x=.65f*x+.35f*car->velocity[0]/speed;z=.65f*z+.35f*car->velocity[2]/speed;
    }
    float heading=(float)atan2((double)x,(double)z);
    if(heading<0)heading+=6.28318530718f;
    return (s16)(u16)(heading*(65536.f/6.28318530718f));
}
static int rocket_camera_loop(struct Camera *c){
    struct MarioState *m=&gMarioStates[0];RocketSnapshot car;
    if(!rocket_camera_selected()||gDjuiInMainMenu||gMarioState!=m||!m->marioObj||!m->area||c->cutscene){rocketChase.valid=0;return 0;}
    if(sCurrPlayMode==PLAY_MODE_PAUSED){
        if(rocketChase.valid&&rocketChase.area==m->area&&rocketChase.epoch==rocket_runtime_epoch()){
            newcam_apply_values(c);return 1;
        }
        return 0;
    }
    if(!rocket_adapter_body_snapshot(m->marioObj,&car)||!rocket_body_pose_valid(&car)){
        rocketChase.valid=0;return 0;
    }
    if(rocketChase.valid&&rocketChase.frame==gGlobalTimer){newcam_apply_values(c);return 1;}
    float jump=0;for(int k=0;k<3;k++){float d=car.position[k]+(k==1?65.f:0)-rocketChase.anchor[k];jump+=d*d;}
    int reset=!rocketChase.valid||rocketChase.area!=m->area||rocketChase.level!=gCurrLevelNum||
        rocketChase.epoch!=rocket_runtime_epoch()||jump>1500.f*1500.f;
    if(reset){
        memset(&rocketChase,0,sizeof rocketChase);rocketChase.valid=1;rocketChase.area=m->area;
        rocketChase.epoch=rocket_runtime_epoch();rocketChase.level=gCurrLevelNum;
        rocketChase.heading=rocket_camera_heading(&car,m->faceAngle[1]);
        for(int k=0;k<3;k++)rocketChase.anchor[k]=car.position[k]+(k==1?65.f:0);
        rocketChase.distance=rocketChase.clearDistance=750.f;
        gNewCamera.yaw=-rocketChase.heading-0x4000;gNewCamera.tilt=1800;
        gNewCamera.yawAccel=gNewCamera.tiltAccel=0;
    }
    rocketChase.frame=gGlobalTimer;
    rocketChase.heading=rocket_camera_heading(&car,rocketChase.heading);
    newcam_stick_input();
    int manual=ABS(gNewCamera.extStick[0])>20||ABS(gNewCamera.extStick[1])>20||
        (gPlayer1Controller->buttonDown&(L_CBUTTONS|R_CBUTTONS|U_CBUTTONS|D_CBUTTONS))||
        (gNewCamera.isMouse&&(mouse_x||mouse_y)&&!gDjuiChatBoxFocus&&!gDjuiConsoleFocus);
    if(manual)rocketChase.lookHold=45;else if(rocketChase.lookHold)rocketChase.lookHold--;
    bool analogue=gNewCamera.isAnalogue;
    gNewCamera.isAnalogue=ABS(gNewCamera.extStick[0])>20||ABS(gNewCamera.extStick[1])>20||
        !(gPlayer1Controller->buttonDown&(L_CBUTTONS|R_CBUTTONS|U_CBUTTONS|D_CBUTTONS));
    newcam_rotate_button();gNewCamera.isAnalogue=analogue;
    gNewCamera.yaw-=gNewCamera.yawAccel*newcam_ivrt(0)*(gNewCamera.sensitivityX/10);
    gNewCamera.tilt=newcam_clamp(gNewCamera.tilt+gNewCamera.tiltAccel*newcam_ivrt(1)*(gNewCamera.sensitivityY/10),-NEWCAM_TILT_LIMIT,NEWCAM_TILT_LIMIT);
    int recenter=!!(gPlayer1Controller->buttonPressed&L_TRIG);
    if(recenter){rocketChase.lookHold=0;gNewCamera.yawAccel=gNewCamera.tiltAccel=0;}
    if(!rocketChase.lookHold){
        s16 target=-rocketChase.heading-0x4000,difference=(s16)(target-gNewCamera.yaw);
        gNewCamera.yaw+=(s16)(difference*(recenter?.35f:.12f));
        gNewCamera.tilt+=(s16)((1800-gNewCamera.tilt)*.08f);
    }
    float speed=hypotf(car.velocity[0],car.velocity[2]);
    rocketChase.distance+=(750.f+fminf(speed/(4600.f*rocket_speed_scale()),1.f)*140.f-rocketChase.distance)*.15f;
    for(int k=0;k<3;k++){
        float desired=car.position[k]+(k==1?65.f:0);
        rocketChase.anchor[k]+=(desired-rocketChase.anchor[k])*(k==1?.4f:.55f);
        gNewCamera.lookAt[k]=gNewCamera.posTarget[k]=rocketChase.anchor[k];
    }
    float horizontal=rocketChase.distance*coss(gNewCamera.tilt);
    gNewCamera.pos[0]=gNewCamera.lookAt[0]+horizontal*coss(gNewCamera.yaw);
    gNewCamera.pos[1]=gNewCamera.lookAt[1]+rocketChase.distance*sins(gNewCamera.tilt);
    gNewCamera.pos[2]=gNewCamera.lookAt[2]+horizontal*sins(gNewCamera.yaw);
    gNewCamera.panX=gNewCamera.panZ=0;newcam_level_bounds();
    /* Reuse native multi-ray clearance and add a central occlusion ray so a
     * narrow wall need not block all three shoulder rays before shortening. */
    newcam_collision();
    Vec3f ray,hit;struct Surface *surface=NULL;
    for(int k=0;k<3;k++)ray[k]=gNewCamera.pos[k]-gNewCamera.lookAt[k];
    find_surface_on_ray(gNewCamera.lookAt,ray,&surface,hit,3.f);
    if(surface){
        float length=sqrtf(ray[0]*ray[0]+ray[1]*ray[1]+ray[2]*ray[2]);
        float distance=0;for(int k=0;k<3;k++){float d=hit[k]-gNewCamera.lookAt[k];distance+=d*d;}
        float fraction=length>1.f?fmaxf(0,sqrtf(distance)-20.f)/length:0;
        for(int k=0;k<3;k++)gNewCamera.pos[k]=gNewCamera.lookAt[k]+ray[k]*fraction;
    }
    float clear=0;for(int k=0;k<3;k++){ray[k]=gNewCamera.pos[k]-gNewCamera.lookAt[k];clear+=ray[k]*ray[k];}clear=sqrtf(clear);
    rocketChase.clearDistance=fminf(clear,rocketChase.clearDistance+35.f);
    if(clear>1.f)for(int k=0;k<3;k++)gNewCamera.pos[k]=gNewCamera.lookAt[k]+ray[k]*rocketChase.clearDistance/clear;
    gNewCamera.distance=rocketChase.distance;newcam_apply_values(c);return 1;
}
