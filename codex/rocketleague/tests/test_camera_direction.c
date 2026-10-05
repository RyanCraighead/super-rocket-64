/* Actual SDL input and native free-camera math; rendering/collision are inert. */
#define main sdl_suite_main
#include "test_bindings_sdl.c"
#undef main
#include "sm64.h"
#include "game/mario.h"
#include "game/camera.h"
#include "engine/math_util.h"
#include "audio/external.h"
#include "game/sound_init.h"
#include "seq_ids.h"
#define NEWCAM_ACCEL 10
#define NEWCAM_ACCEL_MAX 100
#define NEWCAM_YAW_STEP 0x3000
#define NEWCAM_TILT_LIMIT 0x3000
#define NEWCAM_TILT_CENTERING 3000
#define NEWCAM_MARIO_HEIGHT 125
static struct MarioState cameraMario;
static struct PlayerCameraState cameraStatus;
static struct Controller cameraController;
struct Controller *gPlayer1Controller=&cameraController;
struct MarioState *gMarioState=&cameraMario;
struct LakituState gLakituState;
Vec3f gGlobalSoundSource;
void play_sound(s32 bits,f32 *position){(void)bits;(void)position;}
s16 approach_s16_symmetric(s16 current,s16 target,s16 increment){(void)target;(void)increment;assert(!"unexpected camera centering in look-direction fixture");return current;}
static void newcam_set_pan(void){}
static void newcam_level_bounds(void){}
static void newcam_collision(void){}
static void calc_y_to_curr_floor(f32 *a,f32 b,f32 c,f32 *d,f32 e,f32 f){(void)b;(void)c;(void)e;(void)f;*a=*d=0;}
s32 snap_to_45_degrees(s16 x){return (s16)((x+0x1000)&0xe000);}
#include "native_camera_functions.inc.c"
static void initialize_camera(int heading,int invertX,int invertY) {
    memset(&cameraMario,0,sizeof cameraMario);memset(&cameraStatus,0,sizeof cameraStatus);
    cameraMario.statusForCamera=&cameraStatus;cameraStatus.action=ACT_IDLE;
    memset(&gNewCamera,0,sizeof gNewCamera);gNewCamera.isActive=gNewCamera.isAnalogue=true;
    gNewCamera.yaw=(s16)heading;gNewCamera.distance=750;gNewCamera.sensitivityX=gNewCamera.sensitivityY=50;
    gNewCamera.deceleration=50;gNewCamera.invertX=invertX;gNewCamera.invertY=invertY;
    newcam_position_cam();
}
#ifndef CAMERA_DIRECTION_NO_MAIN
int main(void) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");assert(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));init_ok=true;
    attach();rocket_bindings_reset();poll();unsigned checks=0;
    for(int heading=0;heading<65536;heading+=8192)for(int axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2)for(int inverted=0;inverted<2;inverted++) {
        initialize_camera(heading,DEFAULT_CAMERA_X^inverted,DEFAULT_CAMERA_Y^inverted);
        Mat4 view;mtxf_lookat(view,gNewCamera.pos,gNewCamera.lookAt,0);
        Vec3f original={gNewCamera.lookAt[0]-gNewCamera.pos[0],gNewCamera.lookAt[1]-gNewCamera.pos[1],gNewCamera.lookAt[2]-gNewCamera.pos[2]};
        SDL_JoystickSetVirtualAxis(device,axis?SDL_CONTROLLER_AXIS_RIGHTY:SDL_CONTROLLER_AXIS_RIGHTX,sign*25000);
        poll();gNewCamera.extStick[0]=host_pad.ext_stick_x;gNewCamera.extStick[1]=host_pad.ext_stick_y;
        newcam_rotate_button();newcam_position_cam();
        float projection=0;
        for(int k=0;k<3;k++)projection+=(gNewCamera.lookAt[k]-gNewCamera.pos[k]-original[k])*view[k][axis];
        /* SDL X positive=right; SDL Y negative=up. Compare in the original
         * view's right/up basis, not camera orbit or world-axis signs. */
        int expected=(axis?-sign:sign)*(inverted?-1:1);
        assert(projection*expected>1.f);checks++;
        SDL_JoystickSetVirtualAxis(device,axis?SDL_CONTROLLER_AXIS_RIGHTY:SDL_CONTROLLER_AXIS_RIGHTX,0);poll();
    }
    /* Saved stick inversion composes with camera inversion; menus/other
     * characters and the right-stick steering remap keep their own mapping. */
    initialize_camera(0,DEFAULT_CAMERA_X,DEFAULT_CAMERA_Y);configStick.invertRightX=true;
    SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,25000);poll();assert(host_pad.ext_stick_x>0);checks++;
    configStick.invertRightX=false;car_selected=0;poll();assert(host_pad.ext_stick_x>0);checks++;
    car_selected=1;SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,0);poll();
    configRocketBindings.stick=1;SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,25000);
    assert(poll().steer<0&&host_pad.ext_stick_x==0);checks++;
    controller_sdl_shutdown();SDL_JoystickClose(device);SDL_JoystickDetachVirtual(device_index);SDL_Quit();
    printf("PASS camera look direction: %u checks; actual SDL/free-camera matrices, 8 headings, right/left/up/down, explicit inversion, other characters and steering isolation\n",checks);
    return 0;
}

#endif
