/* Native platform loads, with no car teleport or second displacement path. */
#include "rocket_platform.h"
#include "rocket_adapter.h"
#include "pc/character_net.h"
#include "pc/network/network.h"
#include "pc/cliopts.h"
#include "area.h"
#include "mario.h"
#include "obj_behaviors.h"
#include "object_fields.h"
#include "object_constants.h"
#include "object_list_processor.h"
#include "engine/surface_collision.h"
#include "surface_terrains.h"
#include "behavior_data.h"
#include "behavior_table.h"
#include <math.h>
#include <string.h>
#ifdef ROCKET_CAR_QA
#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#endif

static struct CarLoad {
    struct Object *object[4];
    const BehaviorScript *behavior[4];
    u32 sync_id[4];
    float point[4][3];
} loads[MAX_PLAYERS];

int rocket_platform_car(unsigned index) {
    return index==0?rocket_adapter_car_selected():character_net_is_car(index);
}
static int snapshot(unsigned index,RocketSnapshot *car) {
    if(index==0)return rocket_adapter_platform_snapshot(car);
    CharacterNetState state;
    if(!character_net_support_state(index,&state,NULL))return 0;
    *car=state.car;return 1;
}
static struct Object *wheel_floor(struct MarioState *m,const RocketSnapshot *car,int wheel,float point[3]) {
    if(!car->grounded||!car->wheel_contacts[wheel]||car->basis[7]<.25f)return NULL;
    for(int k=0;k<3;k++)point[k]=car->wheel_position[wheel][k]-car->basis[6+k]*car->wheel_radius[wheel];
    struct Surface *floor=NULL;
    /* Native grate filtering belongs to this player, never the platform or host. */
    struct Object *saved=gCurrentObject;gCurrentObject=m->marioObj;
    float height=find_floor(point[0],point[1]+24.f,point[2],&floor);
    gCurrentObject=saved;
    if(!floor||!floor->object||floor->normal.y<.2f||
       (floor->flags&SURFACE_FLAG_INTANGIBLE)||floor->type==SURFACE_INTANGIBLE||
       floor->type==SURFACE_CAMERA_BOUNDARY||floor->type==SURFACE_RAYCAST||
       !isfinite(height)||fabsf(height-point[1])>24.f)return NULL;
    struct Object *object=floor->object;
    if(!(object->activeFlags&ACTIVE_FLAG_ACTIVE)||!gCurrentArea||
       object->header.gfx.activeAreaIndex!=gCurrentArea->index)return NULL;
    point[1]=height;return object;
}
int rocket_platform_support(struct MarioState *m) {
    if(!m||m->playerIndex>=MAX_PLAYERS)return 0;
    struct CarLoad *load=&loads[m->playerIndex];memset(load,0,sizeof(*load));
    if(!m->marioObj||!rocket_platform_car(m->playerIndex))return 0;
    m->marioObj->platform=NULL;
    RocketSnapshot car;if(!snapshot(m->playerIndex,&car))return 1;
    struct Object *objects[4]={0};int best=0;
    for(int i=0;i<4;i++) {
        objects[i]=wheel_floor(m,&car,i,load->point[i]);
        if(!objects[i])continue;
        load->object[i]=objects[i];load->sync_id[i]=objects[i]->oSyncID;load->behavior[i]=objects[i]->behavior;
        int count=0;for(int j=0;j<=i;j++)count+=objects[i]==objects[j];
        if(count>best){best=count;m->marioObj->platform=objects[i];}
    }
    return 1;
}
void rocket_platform_forget(struct Object *object) {
    if(!object)return;
    for(unsigned i=0;i<MAX_PLAYERS;i++) {
        for(unsigned wheel=0;wheel<4;wheel++)if(loads[i].object[wheel]==object) {
            loads[i].object[wheel]=NULL;loads[i].behavior[wheel]=NULL;loads[i].sync_id[wheel]=0;
        }
        if(gMarioStates[i].marioObj&&gMarioStates[i].marioObj->platform==object)
            gMarioStates[i].marioObj->platform=NULL;
    }
    rocket_adapter_forget_platform(object);
}
void rocket_platform_refresh(void) {
    /* Before terrain clears last frame's surfaces, expire remote leases and
     * switch/warp loads. After the car step native platform update refreshes again. */
    memset(loads,0,sizeof loads);
    for(int i=0;i<MAX_PLAYERS;i++)if(gMarioStates[i].marioObj)rocket_platform_support(&gMarioStates[i]);
}
float rocket_platform_load(struct Object *object,float center[3]) {
    float mass=0;int hasCar=0;memset(center,0,sizeof(float)*3);
    for(int i=0;i<MAX_PLAYERS;i++) {
        struct MarioState *m=&gMarioStates[i];
        if(!m->marioObj||!is_player_active(m))continue;
        if(rocket_platform_car(i)) {
            RocketSnapshot car;if(!snapshot(i,&car))continue;
            for(int wheel=0;wheel<4;wheel++) {
                const struct CarLoad *load=&loads[i];
                if(load->object[wheel]!=object||load->sync_id[wheel]!=object->oSyncID||
                   load->behavior[wheel]!=object->behavior||!(object->activeFlags&ACTIVE_FLAG_ACTIVE))continue;
                /* Each tire carries a quarter of the 2-Mario effective mass. */
                for(int k=0;k<3;k++)center[k]+=.5f*load->point[wheel][k];
                mass+=.5f;hasCar=1;
            }
        } else if(m->marioObj->platform==object) {
            center[0]+=m->marioObj->oPosX;center[1]+=m->marioObj->oPosY;center[2]+=m->marioObj->oPosZ;
            mass+=1;
        }
    }
    if(mass>0)for(int k=0;k<3;k++)center[k]/=mass;
    /* Preserve the legacy averaged torque in Mario-only sessions. */
    if(mass>0&&!hasCar)mass=1;
#ifdef ROCKET_CAR_QA
    if(gGlobalTimer%15==0&&getenv("SM64_ROCKET_PLATFORM_QA"))
        fprintf(stderr,"ROCKET_PLATFORM_LOAD frame=%u sync=%u pos=%.2f,%.2f,%.2f mass=%.2f center=%.2f,%.2f,%.2f\n",
            gGlobalTimer,object->oSyncID,object->oPosX,object->oPosY,object->oPosZ,mass,center[0],center[1],center[2]);
#endif
    return mass;
}
static int platform_behavior(const BehaviorScript *behavior) {
    return behavior&&(behavior==bhvSeesawPlatform||behavior==bhvSwingPlatform||
        behavior==bhvWdwSquareFloatingPlatform||behavior==bhvWdwRectangularFloatingPlatform||
        behavior==bhvJrbFloatingPlatform||behavior==bhvLllTiltingInvertedPyramid||
        behavior==bhvBitfsTiltingInvertedPyramid||behavior==bhvTTC2DRotator||
        behavior==bhvTTCCog||behavior==bhvTTCElevator||behavior==bhvTTCMovingBar||
        behavior==bhvTTCPendulum||behavior==bhvTTCPitBlock||behavior==bhvTTCRotatingSolid||
        behavior==bhvTTCSpinner);
}
static void platform_ownership(u8 *override,u8 *own) {
    *override=TRUE;
    *own=!gNetworkPlayerLocal||get_network_player_smallest_global()==gNetworkPlayerLocal;
}
int rocket_platform_managed(const struct SyncObject *so) {
    return gCLIOpts.characterNet&&so&&so->o&&platform_behavior(so->o->behavior)&&
        so->override_ownership==platform_ownership;
}
int rocket_platform_accept(const struct SyncObject *so,unsigned from) {
    if(!rocket_platform_managed(so))return 1;
    struct NetworkPlayer *owner=get_network_player_smallest_global();
    return owner&&from<MAX_PLAYERS&&owner->localIndex==from;
}
static u32 packet_u32(const u8 *b) {
    return (u32)b[0]|((u32)b[1]<<8)|((u32)b[2]<<16)|((u32)b[3]<<24);
}
int rocket_platform_packet_allowed(const struct Packet *p) {
    if(!gCLIOpts.characterNet||gNetworkType==NT_NONE)return 1;
    if(!p||p->error||p->dataLength>=PACKET_LENGTH||p->cursor+13>p->dataLength)return 0;
    const u8 *b=p->buffer+p->cursor;
    struct SyncObject *so=sync_object_get(packet_u32(b+1));
    const BehaviorScript *behavior=get_behavior_from_id(packet_u32(b+9));
    if(!platform_behavior(behavior)&&(!so||!so->o||!platform_behavior(so->o->behavior)))return 1;
    if(!so||!so->o||!platform_behavior(so->o->behavior)||so->o->behavior!=behavior||
       so->o->oSyncID!=packet_u32(b+1)||so->behavior!=so->o->behavior||
       !(so->o->activeFlags&ACTIVE_FLAG_ACTIVE)||!gNetworkAreaLoaded||!gCurrentArea||
       so->o->header.gfx.activeAreaIndex!=gCurrentArea->index||!p->levelAreaMustMatch||
       p->courseNum!=gCurrCourseNum||p->actNum!=gCurrActStarNum||
       p->levelNum!=gCurrLevelNum||p->areaIndex!=gCurrAreaIndex)return 0;
    struct NetworkPlayer *owner=get_network_player_smallest_global();
    if(!owner||!owner->connected||!owner->currLevelSyncValid||!owner->currAreaSyncValid||
       owner->currCourseNum!=p->courseNum||owner->currActNum!=p->actNum||
       owner->currLevelNum!=p->levelNum||owner->currAreaIndex!=p->areaIndex||b[0]!=owner->globalIndex)return 0;
    if(p->localIndex==0||p->localIndex>=MAX_PLAYERS||!gNetworkPlayers[p->localIndex].connected)return 0;
    /* localIndex is set by the transport. The embedded origin is only a claim. */
    if(gNetworkType==NT_CLIENT&&gNetworkSystem&&gNetworkSystem->requireServerBroadcast)
        return gNetworkPlayerServer&&p->localIndex==gNetworkPlayerServer->localIndex;
    return gNetworkPlayers[p->localIndex].globalIndex==b[0];
}
int rocket_platform_begin(struct Object *object) {
#ifdef ROCKET_CAR_QA
    if(gGlobalTimer%15==0&&getenv("SM64_ROCKET_PLATFORM_QA"))
        fprintf(stderr,"ROCKET_PLATFORM_POSE frame=%u sync=%u pos=%.2f,%.2f,%.2f angles=%d,%d,%d\n",
            gGlobalTimer,object->oSyncID,object->oPosX,object->oPosY,object->oPosZ,
            object->oFaceAnglePitch,object->oFaceAngleYaw,object->oFaceAngleRoll);
#endif
    if(!gCLIOpts.characterNet||!object||!platform_behavior(object->behavior))return 1;
    struct SyncObject *so=sync_object_get(object->oSyncID);
    if(!so)so=sync_object_init(object,SYNC_DISTANCE_INFINITE);
    if(!so)return 0;
    /* Do not replace another subsystem's authority callback. */
    if(so->override_ownership&&so->override_ownership!=platform_ownership)return 0;
    if(so->override_ownership!=platform_ownership) {
        so->override_ownership=platform_ownership;
        so->maxSyncDistance=SYNC_DISTANCE_INFINITE;
        so->minUpdateRate=so->maxUpdateRate=1.f/30.f;
        sync_object_init_field(object,object->oFaceAnglePitch);
        sync_object_init_field(object,object->oFaceAngleYaw);
        sync_object_init_field(object,object->oFaceAngleRoll);
        sync_object_init_field(object,object->oAngleVelPitch);
        sync_object_init_field(object,object->oAngleVelYaw);
        sync_object_init_field(object,object->oAngleVelRoll);
        if((object->behavior==bhvWdwSquareFloatingPlatform||object->behavior==bhvWdwRectangularFloatingPlatform||object->behavior==bhvJrbFloatingPlatform)) {
            sync_object_init_field(object,object->oFloatingPlatformUnkF4);
            sync_object_init_field(object,object->oFloatingPlatformUnkF8);
            sync_object_init_field(object,object->oFloatingPlatformUnk100);
        }
        if(object->behavior==bhvLllTiltingInvertedPyramid||object->behavior==bhvBitfsTiltingInvertedPyramid) {
            sync_object_init_field(object,object->oTiltingPyramidNormalX);
            sync_object_init_field(object,object->oTiltingPyramidNormalY);
            sync_object_init_field(object,object->oTiltingPyramidNormalZ);
        }
    }
    u8 override,own;platform_ownership(&override,&own);return own;
}
