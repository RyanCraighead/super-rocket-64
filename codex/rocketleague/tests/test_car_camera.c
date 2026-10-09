#include "speed_fixture_stubs.h"
#define CAMERA_DIRECTION_NO_MAIN
#define newcam_collision unused_camera_collision
#define newcam_level_bounds unused_camera_bounds
#include "test_camera_direction.c"
#undef newcam_collision
#undef newcam_level_bounds
#undef main
#include "pc/rocket_runtime.h"
#include "game/rocket_adapter.h"
#include "codex/rocketleague/physics/body_contact.h"
#define NEWCAM_COLLISION_CHECK_WIDTH 75
#define NEWCAM_COLLISION_CHECK_HEIGHT 90
struct MarioState gMarioStates[MAX_PLAYERS];
u32 gGlobalTimer;
s16 gCurrLevelNum,gCurrSaveFileNum;
unsigned int configRocketCameraMode=1;
static u32 carEpoch=1;
static RocketSnapshot chaseCar;
static struct Area chaseArea;
static struct Camera camera;
static int bodyReady=1,wallEnabled,rayCalls,applies,freeEnabled;
static struct Surface wall;
static unsigned chaseChecks;
static unsigned cameraSaves;
s32 get_dialog_id(void){return gDialogID;}
const char *configfile_name(void){return "camera-fixture.cfg";}
void configfile_save(const char *name){assert(!strcmp(name,"camera-fixture.cfg"));cameraSaves++;}
#define CHASE_CHECK(x) do{chaseChecks++;if(!(x)){fprintf(stderr,"camera line %d: %s\n",__LINE__,#x);abort();}}while(0)
uint32_t rocket_runtime_epoch(void){return carEpoch;}
int rocket_adapter_car_selected(void){return car_selected;}
int rocket_adapter_body_snapshot(struct Object *o,RocketSnapshot *s){(void)o;if(!bodyReady||gMarioStates[0].freeze)return 0;*s=chaseCar;return 1;}
static void newcam_toggle(bool enabled){gNewCamera.isActive=enabled;}
bool camera_config_is_free_cam_enabled(void){return freeEnabled;}
void newcam_init_settings(void){}
static void newcam_level_bounds(void){}
/* Plane-ray service; the actual native multi-ray collision algorithm is below. */
void find_surface_on_ray(Vec3f origin,Vec3f direction,struct Surface **surface,Vec3f hit,float precision){
 CHASE_CHECK(precision==3);rayCalls++;*surface=NULL;
 float t=1;
 if(wallEnabled&&direction[2]<0&&origin[2]>-300){float crossing=(-300-origin[2])/direction[2];if(crossing>=0&&crossing<=1){t=crossing;*surface=&wall;}}
 for(int k=0;k<3;k++)hit[k]=origin[k]+direction[k]*t;
}
static void newcam_apply_values(struct Camera *c){applies++;vec3f_copy(c->pos,gNewCamera.pos);vec3f_copy(c->focus,gNewCamera.lookAt);c->yaw=-gNewCamera.yaw+0x4000;}
#include "car_camera_native.inc.h"
#include "game/rocket_camera.inc.h"
static void chase_setup(int heading){
 memset(&rocketChase,0,sizeof rocketChase);memset(&camera,0,sizeof camera);memset(&chaseCar,0,sizeof chaseCar);
 memset(gMarioStates,0,sizeof gMarioStates);gMarioState=&gMarioStates[0];
 static struct Object player;gMarioState->marioObj=&player;gMarioState->area=&chaseArea;gMarioState->statusForCamera=&cameraStatus;
 chaseArea.camera=&camera;
 gMarioState->health=0x880;gMarioState->action=ACT_IDLE;
 gPlayer1Controller=&cameraController;memset(&cameraController,0,sizeof cameraController);
 memset(&gNewCamera,0,sizeof gNewCamera);gNewCamera.isActive=gNewCamera.isAnalogue=true;gNewCamera.deceleration=50;
 gNewCamera.sensitivityX=gNewCamera.sensitivityY=50;gNewCamera.invertX=DEFAULT_CAMERA_X;gNewCamera.invertY=DEFAULT_CAMERA_Y;
 car_selected=car_enabled=car_drawable=bodyReady=1;configRocketCameraMode=1;carEpoch=1;gGlobalTimer=100;
 sCurrPlayMode=PLAY_MODE_NORMAL;wallEnabled=0;wall.normal.z=1;applies=rayCalls=0;gCurrLevelNum=9;
 chaseCar.position[1]=40;chaseCar.basis[0]=sins(heading);chaseCar.basis[2]=coss(heading);
 chaseCar.basis[3]=coss(heading);chaseCar.basis[5]=-sins(heading);chaseCar.basis[7]=1;
 CHASE_CHECK(rocket_camera_loop(&camera));
}
static void frame(void){gGlobalTimer++;CHASE_CHECK(rocket_camera_loop(&camera));}
static float distance(void){float d=0;for(int k=0;k<3;k++){float x=camera.pos[k]-camera.focus[k];d+=x*x;}return sqrtf(d);}
static void cycle_poll(void){
 poll();cameraController.buttonDown=host_pad.button;rocket_camera_sync();
}
static void modal_camera_poll(void){
 u16 previous=cameraController.buttonDown;poll();
 cameraController.buttonDown=host_pad.button;
 cameraController.buttonPressed=host_pad.button&~previous;
 u16 down=cameraController.buttonDown,pressed=cameraController.buttonPressed;
 struct RocketCameraInputGuard guard=rocket_camera_input_begin(&camera);
 rocket_camera_sync();if(!rocket_camera_selected())newcam_zoom_button();
 rocket_camera_input_end(guard);
 CHASE_CHECK(cameraController.buttonDown==down&&cameraController.buttonPressed==pressed);
}
static void dialog_camera_checks(void){
 rocket_bindings_reset();controller_sdl_rocket_bindings_changed();chase_setup(0);
 configRocketCameraMode=0;freeEnabled=1;
 configKeyR[0]=VK_RTRIGGER;configKeyR[1]=VK_BASE_SDL_GAMEPAD+SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
 controller_sdl_bind();modal_camera_poll();
 gDialogID=0;car_drawable=0;modal_camera_poll();
 int distanceIndex=gNewCamera.distanceTargetIndex;unsigned before=cameraSaves;
 trigger(32767);modal_camera_poll();
 CHASE_CHECK(gNewCamera.distanceTargetIndex==distanceIndex&&cameraSaves==before);
 // Presses after opening are also blocked, as are legacy R1 and keyboard R.
 trigger(-32768);modal_camera_poll();button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);modal_camera_poll();
 CHASE_CHECK(gNewCamera.distanceTargetIndex==distanceIndex);
 button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);modal_camera_poll();
 configKeyR[2]=0x13;keyboard_bindkeys();keyboard_on_key_down(0x13);modal_camera_poll();
 CHASE_CHECK(gNewCamera.distanceTargetIndex==distanceIndex);keyboard_on_key_up(0x13);modal_camera_poll();
 // Remapped R2 confirmation survives the camera-only mask on every page.
 configRocketBindings.action[RA_JUMP]=RB_RT;controller_sdl_rocket_bindings_changed();modal_camera_poll();
 for(int page=0;page<3;page++){
  trigger(32767);modal_camera_poll();CHASE_CHECK(cameraController.buttonPressed&A_BUTTON);
  CHASE_CHECK(gNewCamera.distanceTargetIndex==distanceIndex&&cameraSaves==before);
  if(page<2){trigger(-32768);modal_camera_poll();}
 }
 gDialogID=DIALOG_NONE;car_drawable=1;
 for(int i=0;i<5;i++){modal_camera_poll();CHASE_CHECK(!(cameraController.buttonDown&A_BUTTON)&&cameraSaves==before&&gNewCamera.distanceTargetIndex==distanceIndex);}
 trigger(-32768);modal_camera_poll();
 // Normal R1 zoom returns only after a fresh release/press.
 button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);modal_camera_poll();
 CHASE_CHECK(gNewCamera.distanceTargetIndex==(distanceIndex+1)%(int)NEWCAM_NUM_DISTANCES);
 button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);modal_camera_poll();
 rocket_bindings_reset();controller_sdl_rocket_bindings_changed();modal_camera_poll();
 // Both modal sources plus the action-only transition before a cutscene starts.
 const u32 actions[]={ACT_READING_NPC_DIALOG,ACT_READING_SIGN,ACT_WAITING_FOR_DIALOG,ACT_PULLING_DOOR,ACT_PUSHING_DOOR};
 for(unsigned i=0;i<sizeof actions/sizeof *actions;i++){
  configRocketCameraMode=0;gMarioState->action=actions[i];car_drawable=0;
  distanceIndex=gNewCamera.distanceTargetIndex;before=cameraSaves;
  button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);modal_camera_poll();
  CHASE_CHECK(gNewCamera.distanceTargetIndex==distanceIndex);
  gMarioState->action=ACT_IDLE;car_drawable=1;
  for(int held=0;held<3;held++){modal_camera_poll();CHASE_CHECK(gNewCamera.distanceTargetIndex==distanceIndex&&cameraSaves==before);}
  button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);modal_camera_poll();
  button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);modal_camera_poll();
  CHASE_CHECK(gNewCamera.distanceTargetIndex==(distanceIndex+1)%(int)NEWCAM_NUM_DISTANCES);
  button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);modal_camera_poll();
 }
 // A camera event sampled before the dialogue opens must not reach dispatch;
 // queued pressed bits remain blocked across close until a neutral observation.
 gDialogID=0;distanceIndex=gNewCamera.distanceTargetIndex;before=cameraSaves;
 cameraController.buttonDown=cameraController.buttonPressed=R_TRIG|Y_BUTTON|A_BUTTON|B_BUTTON;
 struct RocketCameraInputGuard guard=rocket_camera_input_begin(&camera);
 CHASE_CHECK(cameraController.buttonPressed==(A_BUTTON|B_BUTTON));newcam_zoom_button();rocket_camera_sync();rocket_camera_input_end(guard);
 gDialogID=DIALOG_NONE;
 guard=rocket_camera_input_begin(&camera);newcam_zoom_button();rocket_camera_sync();rocket_camera_input_end(guard);
 CHASE_CHECK(gNewCamera.distanceTargetIndex==distanceIndex&&cameraSaves==before);
 cameraController.buttonDown=cameraController.buttonPressed=0;
 guard=rocket_camera_input_begin(&camera);rocket_camera_sync();rocket_camera_input_end(guard);
 // Deliberately map car camera to physical R2: it cycles the car view, not R zoom.
 configRocketBindings.action[RA_CAMERA]=RB_RT;controller_sdl_rocket_bindings_changed();modal_camera_poll();
 gDialogID=0;modal_camera_poll();trigger(32767);modal_camera_poll();
 CHASE_CHECK(cameraSaves==before&&gNewCamera.distanceTargetIndex==distanceIndex);
 gDialogID=DIALOG_NONE;modal_camera_poll();CHASE_CHECK(cameraSaves==before);
 trigger(-32768);modal_camera_poll();trigger(32767);modal_camera_poll();
 CHASE_CHECK(cameraSaves==before+1&&configRocketCameraMode==1&&gNewCamera.distanceTargetIndex==distanceIndex);
 trigger(-32768);modal_camera_poll();
 // Other characters retain native R input.
 car_selected=0;gDialogID=0;cameraController.buttonDown=cameraController.buttonPressed=R_TRIG;
 guard=rocket_camera_input_begin(&camera);CHASE_CHECK(cameraController.buttonPressed&R_TRIG);rocket_camera_input_end(guard);
 car_selected=1;gDialogID=DIALOG_NONE;configKeyR[2]=VK_INVALID;keyboard_bindkeys();
 rocket_bindings_reset();controller_sdl_rocket_bindings_changed();modal_camera_poll();freeEnabled=0;
 puts("PASS modal camera: actual SDL R2/R1 and keyboard, native zoom, mapped camera, dialogue confirm/held-close/queued input, sign/NPC/door actions and fresh re-press");
}
static void camera_cycle_checks(void){
 static const int keys[]={-1,0,1,2,3,9,10,7,8,11,12,13,14};
 for(int binding=RB_SOUTH;binding<RB_COUNT;binding++){
  rocket_bindings_reset();configRocketBindings.action[RA_CAMERA]=binding;controller_sdl_rocket_bindings_changed();
  chase_setup(0);cycle_poll();unsigned before=cameraSaves;
  if(binding==RB_LT)SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);
  else if(binding==RB_RT)SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);
  else button((SDL_GameControllerButton)keys[binding],1);
  cycle_poll();CHASE_CHECK(configRocketCameraMode==0&&cameraSaves==before+1&&(host_pad.button&Y_BUTTON));
  for(int i=0;i<5;i++){cycle_poll();}CHASE_CHECK(cameraSaves==before+1);
  SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
  if(binding<RB_LT)button((SDL_GameControllerButton)keys[binding],0);
  cycle_poll();
 }
 rocket_bindings_reset();controller_sdl_rocket_bindings_changed();chase_setup(0);cycle_poll();
 for(int gate=0;gate<9;gate++){
  unsigned before=cameraSaves,mode=configRocketCameraMode;
  switch(gate){case 0:panel_active=1;break;case 1:focused=false;break;case 2:sCurrPlayMode=PLAY_MODE_PAUSED;break;
   case 3:camera.cutscene=1;break;case 4:gMarioState->freeze=1;break;case 5:car_drawable=0;break;
   case 6:gDialogID=1;break;case 7:car_selected=0;break;case 8:wheel_open=1;break;}
  button(SDL_CONTROLLER_BUTTON_Y,1);cycle_poll();CHASE_CHECK(cameraSaves==before&&configRocketCameraMode==mode);
  panel_active=0;focused=true;sCurrPlayMode=PLAY_MODE_NORMAL;camera.cutscene=0;gMarioState->freeze=0;
  car_drawable=car_selected=1;gDialogID=DIALOG_NONE;wheel_open=0;
  cycle_poll();CHASE_CHECK(cameraSaves==before);
  button(SDL_CONTROLLER_BUTTON_Y,0);cycle_poll();button(SDL_CONTROLLER_BUTTON_Y,1);cycle_poll();
  CHASE_CHECK(cameraSaves==before+1&&configRocketCameraMode==1-mode);
  button(SDL_CONTROLLER_BUTTON_Y,0);cycle_poll();
 }
 // Rebind during a held press requires release, including a shared old key.
 button(SDL_CONTROLLER_BUTTON_Y,1);cycle_poll();unsigned before=cameraSaves;
 configRocketBindings.action[RA_CAMERA]=RB_RB;controller_sdl_rocket_bindings_changed();button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);
 cycle_poll();CHASE_CHECK(cameraSaves==before);button(SDL_CONTROLLER_BUTTON_Y,0);button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);cycle_poll();
 button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1);cycle_poll();CHASE_CHECK(cameraSaves==before+1&&!(host_pad.button&R_TRIG));
 button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0);cycle_poll();
 configRocketBindings.action[RA_CAMERA]=RB_NONE;controller_sdl_rocket_bindings_changed();cycle_poll();before=cameraSaves;
 button(SDL_CONTROLLER_BUTTON_Y,1);cycle_poll();CHASE_CHECK(cameraSaves==before);button(SDL_CONTROLLER_BUTTON_Y,0);cycle_poll();
 // Keyboard uses the already-saved Y action, independently of an unbound pad.
 configKeyY[0]=0x32;keyboard_bindkeys();keyboard_on_key_down(0x32);cycle_poll();CHASE_CHECK(cameraSaves==before+1);
 for(int i=0;i<5;i++){cycle_poll();}CHASE_CHECK(cameraSaves==before+1);keyboard_on_key_up(0x32);cycle_poll();
 configKeyY[0]=0x21;keyboard_bindkeys();keyboard_on_key_down(0x32);cycle_poll();CHASE_CHECK(cameraSaves==before+1);keyboard_on_key_up(0x32);
 keyboard_on_key_down(0x21);cycle_poll();CHASE_CHECK(cameraSaves==before+2);keyboard_on_key_up(0x21);cycle_poll();
 rocket_bindings_reset();puts("PASS camera cycle: actual SDL and keyboard, all remaps, two views, one save per fresh press, blocked/held/unbound gates");
}
int main(void){
 initialize_camera(0,DEFAULT_CAMERA_X,DEFAULT_CAMERA_Y);
 SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");CHASE_CHECK(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));init_ok=true;attach();rocket_bindings_reset();poll();
 dialog_camera_checks();
 for(int heading=0;heading<65536;heading+=8192)for(int axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2)for(int invert=0;invert<2;invert++){
  chase_setup(heading);gNewCamera.invertX=DEFAULT_CAMERA_X^(axis==0&&invert);gNewCamera.invertY=DEFAULT_CAMERA_Y^(axis==1&&invert);
  Mat4 view;mtxf_lookat(view,camera.pos,camera.focus,0);Vec3f original;for(int k=0;k<3;k++)original[k]=camera.focus[k]-camera.pos[k];
  SDL_JoystickSetVirtualAxis(device,axis?SDL_CONTROLLER_AXIS_RIGHTY:SDL_CONTROLLER_AXIS_RIGHTX,sign*25000);poll();
  cameraController.extStickX=host_pad.ext_stick_x;cameraController.extStickY=host_pad.ext_stick_y;frame();
  float projection=0;for(int k=0;k<3;k++)projection+=(camera.focus[k]-camera.pos[k]-original[k])*view[k][axis];
  int expected=(axis?-sign:sign)*(invert?-1:1);CHASE_CHECK(projection*expected>0);
  SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTX,0);SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_RIGHTY,0);poll();
 }
 chase_setup(0);s16 yaw=gNewCamera.yaw;chaseCar.velocity[2]=-1200;for(int i=0;i<60;i++)frame();CHASE_CHECK(gNewCamera.yaw==yaw);
 /* Horizon/heading stay stable during somersault and inverted air roll. */
 chaseCar.flipping=1;chaseCar.basis[2]=-1;chaseCar.basis[7]=-1;for(int i=0;i<15;i++)frame();CHASE_CHECK(gNewCamera.yaw==yaw&&gNewCamera.tilt==1800);
 chase_setup(0);cameraController.extStickX=-70;frame();s16 looked=gNewCamera.yaw;CHASE_CHECK(looked!=yaw);
 cameraController.extStickX=0;for(int i=0;i<60;i++)frame();CHASE_CHECK(abs((s16)(gNewCamera.yaw-yaw))<abs((s16)(looked-yaw)));
 cameraController.buttonPressed=L_TRIG;frame();CHASE_CHECK(!rocketChase.lookHold);cameraController.buttonPressed=0;
 /* Boost changes distance smoothly; no trigger may cycle Mario zoom here. */
 chase_setup(0);float startDistance=distance();chaseCar.velocity[2]=4600;frame();CHASE_CHECK(distance()>startDistance&&distance()<890);
 for(int i=0;i<40;i++){frame();}CHASE_CHECK(distance()>885&&distance()<=890.1f);
 for(unsigned percent=50;percent<=100;percent+=25){
  fixtureSpeedPercent=percent;chase_setup(0);chaseCar.velocity[2]=4600.f*rocket_speed_scale();
  for(int i=0;i<40;i++){frame();}CHASE_CHECK(distance()>885&&distance()<=890.1f);
 }
 fixtureSpeedPercent=100;
 unsigned zoom=gNewCamera.distanceTargetIndex;SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);RocketInput drive=poll();
 CHASE_CHECK(drive.throttle>.9f&&!(host_pad.button&R_TRIG));cameraController.buttonPressed=host_pad.button;frame();CHASE_CHECK((unsigned)gNewCamera.distanceTargetIndex==zoom);
 SDL_JoystickSetVirtualAxis(device,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);poll();
 chase_setup(0);wallEnabled=1;frame();CHASE_CHECK(camera.pos[2]>-300&&distance()<400&&rayCalls>4);
 float shortened=distance();wallEnabled=0;frame();CHASE_CHECK(distance()>shortened&&distance()<=shortened+35.1f);
 for(int i=0;i<30;i++){frame();}CHASE_CHECK(distance()>740);
 /* Pause freezes the camera and native handoffs/selection changes release it. */
 chase_setup(0);struct RocketChaseCamera before=rocketChase;Vec3f position;vec3f_copy(position,camera.pos);
 sCurrPlayMode=PLAY_MODE_PAUSED;cameraController.extStickX=80;gGlobalTimer++;CHASE_CHECK(rocket_camera_loop(&camera));
 CHASE_CHECK(!memcmp(&before,&rocketChase,sizeof before)&&!memcmp(position,camera.pos,sizeof position));
 sCurrPlayMode=PLAY_MODE_NORMAL;cameraController.extStickX=0;chaseCar.position[0]=4000;carEpoch++;frame();CHASE_CHECK(rocketChase.anchor[0]==4000);
 bodyReady=0;CHASE_CHECK(!rocket_camera_loop(&camera));CHASE_CHECK(!rocketChase.valid);bodyReady=1;frame();
 camera.cutscene=1;CHASE_CHECK(!rocket_camera_loop(&camera));camera.cutscene=0;frame();
 gMarioState=&gMarioStates[1];CHASE_CHECK(!rocket_camera_loop(&camera));gMarioState=&gMarioStates[0];frame();
 configRocketCameraMode=0;CHASE_CHECK(!rocket_camera_loop(&camera));rocket_camera_sync();CHASE_CHECK(!gNewCamera.isActive);
 configRocketCameraMode=1;rocket_camera_sync();CHASE_CHECK(gNewCamera.isActive);car_selected=0;rocket_camera_sync();CHASE_CHECK(!gNewCamera.isActive);
 camera_cycle_checks();
 printf("PASS %u native-input car-camera checks: direction, reverse/roll, recenter, boost/trigger, collision, pause/warp and local ownership\n",chaseChecks);
 SDL_JoystickClose(device);SDL_JoystickDetachVirtual(device_index);SDL_Quit();return 0;
}
