#include "rocket_lava.h"
#include "rocket_adapter.h"
#include "sm64.h"
#include "mario.h"
#include "area.h"
#include "level_update.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "pc/rocket_runtime.h"
#include "../../codex/rocketleague/physics/body_contact.h"
#include <math.h>

static int burning_below(const float point[3], float radius) {
    if(!isfinite(radius)||radius<0||radius>64.f)return 0;
    for(int k=0;k<3;k++)if(!isfinite(point[k]))return 0;
    struct Surface *floor=NULL;
    float height=find_floor(point[0],point[1]+8.f,point[2],&floor);
    return floor&&floor->type==SURFACE_BURNING&&floor->normal.y>.01f&&
        isfinite(height)&&fabsf(point[1]-radius-height)<8.f;
}
int rocket_lava_floor_contact(struct MarioState *m) {
    RocketSnapshot car;
    if(!m||m->playerIndex||!rocket_adapter_body_snapshot(m->marioObj,&car)||
       !rocket_body_pose_valid(&car))return 0;
    /* Query the real floor below each contacting tire, not Mario's chassis
     * origin. Airborne/roof-down cars can also touch with the oriented body. */
    for(int i=0;i<4;i++)if(car.wheel_contacts[i]&&car.wheel_radius[i]>0&&
        burning_below(car.wheel_position[i],car.wheel_radius[i]))return 1;
    const float half[3]={ROCKET_BODY_HALF_LENGTH,ROCKET_BODY_HALF_WIDTH,ROCKET_BODY_HALF_HEIGHT};
    for(int v=0;v<8;v++){
        float point[3];
        for(int k=0;k<3;k++){
            point[k]=car.position[k]+car.basis[k]*ROCKET_BODY_FORWARD_OFFSET+
                car.basis[6+k]*ROCKET_BODY_UP_OFFSET;
            for(int axis=0;axis<3;axis++)point[k]+=car.basis[3*axis+k]*half[axis]*((v&(1<<axis))?1.f:-1.f);
        }
        if(burning_below(point,0))return 1;
    }
    return 0;
}
void rocket_lava_start(struct MarioState *m,unsigned actionArg) {
    RocketSnapshot car;
    if(!m||m->playerIndex||!rocket_adapter_body_snapshot(m->marioObj,&car))return;
    float speed=hypotf(car.velocity[0],car.velocity[2])/30.f;
    if(!isfinite(speed))return;
    /* Preserve travel direction, including a reverse approach. A stationary
     * floor hit escapes forward. Wall lava retains the native outward yaw. */
    if(!actionArg&&speed>=1.f)m->faceAngle[1]=atan2s(car.velocity[2],car.velocity[0]);
    m->forwardVel=fminf(32.f,fmaxf(16.f,speed));
    m->vel[0]=m->slideVelX=m->forwardVel*sins(m->faceAngle[1]);
    m->vel[2]=m->slideVelZ=m->forwardVel*coss(m->faceAngle[1]);
}
int rocket_lava_update(struct MarioState *m) {
    if(!m||m->playerIndex||m->action!=ACT_LAVA_BOOST||!m->controller||
       !m->area||!rocket_adapter_car_selected())return 0;
    RocketInput keyboard={0},input={0};
    if(!m->freeze&&sCurrPlayMode!=PLAY_MODE_PAUSED&&m->health>=0x100){
        keyboard.throttle=m->controller->rawStickY/80.f;
        keyboard.steer=-m->controller->rawStickX/80.f;
        rocket_runtime_read_selected_input(&keyboard,&input);
    }
    float throttle=isfinite(input.throttle)?fminf(1.f,fmaxf(-1.f,input.throttle)):0;
    float steer=isfinite(input.steer)?fminf(1.f,fmaxf(-1.f,input.steer)):0;
    if(!isfinite(m->forwardVel))m->forwardVel=0;
    /* Native-scale escape speed, with no boost, jump or air-roll force. Neutral
     * input coasts as Mario does; accelerator and brake stay remappable. */
    if(throttle!=0)m->forwardVel+=throttle;
    else m->forwardVel=approach_f32(m->forwardVel,0,.35f,.35f);
    m->forwardVel=fminf(32.f,fmaxf(0,m->forwardVel));
    m->faceAngle[1]+=(s16)(steer*512.f);
    m->vel[0]=m->slideVelX=m->forwardVel*sins(m->faceAngle[1]);
    m->vel[2]=m->slideVelZ=m->forwardVel*coss(m->faceAngle[1]);
    return 1;
}
