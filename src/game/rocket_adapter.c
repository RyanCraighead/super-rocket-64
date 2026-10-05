/* RocketSim reconstruction hosted in SM64. No original RL physics claim. */
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "behavior_data.h"
#include "area.h"
#include "display.h"
#include "engine/math_util.h"
#include "engine/surface_load.h"
#include "interaction.h"
#include "level_update.h"
#include "hardcoded.h"
#include "mario.h"
#include "object_fields.h"
#include "object_helpers.h"
#include "object_list_processor.h"
#include "rocket_adapter.h"
#include "rocket_caps.h"
#include "../../codex/rocketleague/physics/body_contact.h"
#include "../../codex/rocketleague/physics/vanish_collision.h"
#include "rocket_water.h"
#include "engine/surface_collision.h"
#include "rocket_environment.h"
#include "pc/rocket_runtime.h"
#include "pc/network/network.h"
#include "pc/cliopts.h"
#include "level_table.h"

static struct MarioState *player;
static struct Area *area;
static s16 level;
static int ownHide,haveFrame;
static int textJumpHeld=1,textJumpPressed,textHaveFrame;
static u32 textInputFrame;
static int selected = 1; /* Standalone launch retains ownership by default. */
static u32 previousFrame;
static uint64_t frame,meshHashes[2];
static Vec3f lastPosition;
static int phaseActive,haveClearPose;
static RocketSnapshot clearPose;
static void whomp_crush_forget(struct Object *object);
static int native_phase_surface(const struct Surface *surface) {
    return surface&&surface->type==SURFACE_VANISH_CAP_WALLS&&
        (gLevelValues.fixVanishFloors||fabsf(surface->normal.y)<=.01f);
}
static int native_water(float x,float z,float *height){
    *height=find_water_level(x,z);return *height>gLevelValues.floorLowerLimit;
}
static int solid_surface(const struct Surface *surface) {
    return surface&&!(surface->flags&SURFACE_FLAG_INTANGIBLE)&&
        surface->type!=SURFACE_INTANGIBLE&&surface->type!=SURFACE_CAMERA_BOUNDARY&&
        surface->type!=SURFACE_RAYCAST;
}
static struct Surface *phase_overlap(const RocketSnapshot *pose,int allSolid) {
    for(int dynamic=0;dynamic<2;dynamic++){
        SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
        for(int x=0;x<NUM_CELLS;x++)for(int z=0;z<NUM_CELLS;z++)for(int p=0;p<3;p++)
            for(struct SurfaceNode *node=partition[x][z][p].next;node;node=node->next){
                const struct Surface *s=node->surface;
                if(!solid_surface(s)||(!allSolid&&!native_phase_surface(s))||(allSolid==2&&native_phase_surface(s)))continue;
                float triangle[3][3];const s16 *v[]={s->vertex1,s->vertex2,s->vertex3};
                for(int i=0;i<3;i++)for(int k=0;k<3;k++)triangle[i][k]=v[i][k];
                if(rocket_car_triangle_overlap(pose,triangle,native_phase_surface(s)?2.f:-2.f))return node->surface;
            }
    }
    return NULL;
}
static int phase_clear_pose(const RocketSnapshot *pose,RocketSnapshot *out) {
    struct Surface *surface=phase_overlap(pose,0);
    if(!surface){*out=*pose;return 1;}
    if(haveClearPose&&!phase_overlap(&clearPose,1)){*out=clearPose;return 1;}
    // A native cap animation can resume with the larger chassis already touching
    // a grate. Find the nearest short exit along that grate's authored normal.
    // Every candidate and intermediate box is checked against ordinary geometry.
    const float normal[3]={surface->normal.x,surface->normal.y,surface->normal.z};
    int blocked[2]={0,0};
    for(int distance=8;distance<=384;distance+=8)for(int sign=-1;sign<=1;sign+=2){
        int direction=sign>0;if(blocked[direction])continue;
        RocketSnapshot candidate=*pose;
        for(int k=0;k<3;k++)candidate.position[k]=pose->position[k]+normal[k]*distance*sign;
        if(phase_overlap(&candidate,2)){blocked[direction]=1;continue;}
        if(!phase_overlap(&candidate,1)){*out=candidate;return 1;}
    }
    return 0;
}
static int pointer_compare(const void *a,const void *b) {
    const struct Surface *sa=*(const struct Surface *const *)a,*sb=*(const struct Surface *const *)b;
    uintptr_t x=(uintptr_t)sa->object,y=(uintptr_t)sb->object;
    if(x!=y)return (x>y)-(x<y);
    x=(uintptr_t)sa;y=(uintptr_t)sb;
    return (x>y)-(x<y);
}
static struct PlatformIdentity {
    struct Object *object;
    const BehaviorScript *behavior;
    const Collision *collision;
    u32 sync;
    uint64_t id;
    int seen;
} platformIdentities[1024];
static uint64_t nextPlatformId;
void rocket_adapter_forget_platform(struct Object *object) {
    whomp_crush_forget(object);
    for(size_t i=0;i<1024;i++)if(platformIdentities[i].object==object)
        memset(&platformIdentities[i],0,sizeof platformIdentities[i]);
}
static uint64_t platform_identity(struct Object *object) {
    struct PlatformIdentity *freeSlot=NULL;
    for(size_t i=0;i<1024;i++) {
        struct PlatformIdentity *entry=&platformIdentities[i];
        if(!entry->object) { if(!freeSlot)freeSlot=entry;continue; }
        if(entry->object!=object)continue;
        if(entry->behavior!=object->behavior||entry->collision!=object->collisionData||entry->sync!=object->oSyncID)
            entry->id=++nextPlatformId;
        entry->behavior=object->behavior;entry->collision=object->collisionData;entry->sync=object->oSyncID;
        entry->seen=1;return entry->id;
    }
    if(!freeSlot)return 0;
    *freeSlot=(struct PlatformIdentity){object,object->behavior,object->collisionData,object->oSyncID,++nextPlatformId,1};
    return freeSlot->id;
}
static int sync_platforms(struct Surface **surfaces,RocketTriangle *triangles,size_t count) {
    RocketPlatform *platforms=calloc(1024,sizeof(*platforms));
    if(!platforms)return 0;
    size_t groups=0,orphan=0;
    for(size_t i=0;i<1024;i++)platformIdentities[i].seen=0;
    for(size_t i=0;i<count;) {
        struct Object *object=surfaces[i]->object;
        size_t end=i+1;while(end<count&&surfaces[end]->object==object)end++;
        if(!object) { orphan=end;i=end;continue; }
        if(groups==1024){free(platforms);return 0;}
        RocketPlatform *platform=&platforms[groups++];
        platform->object_id=platform_identity(object);
        if(!platform->object_id){free(platforms);return 0;}
        for(int k=0;k<3;k++) {
            platform->position[k]=object->transform[3][k];
            for(int axis=0;axis<3;axis++)platform->basis[axis*3+k]=object->transform[axis][k];
        }
        platform->triangles=triangles+i;platform->count=end-i;
        for(size_t t=i;t<end;t++)for(int v=0;v<3;v++) {
            float relative[3];for(int k=0;k<3;k++)relative[k]=triangles[t].v[v][k]-platform->position[k];
            for(int axis=0;axis<3;axis++) {
                float local=0;for(int k=0;k<3;k++)local+=relative[k]*platform->basis[axis*3+k];
                triangles[t].v[v][axis]=local;
            }
        }
        i=end;
    }
    for(size_t i=0;i<1024;i++)if(!platformIdentities[i].seen)memset(&platformIdentities[i],0,sizeof(platformIdentities[i]));
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<orphan;i++)for(int v=0;v<3;v++)for(int k=0;k<3;k++) {
        hash^=(uint16_t)triangles[i].v[v][k];hash*=UINT64_C(1099511628211);
    }
    for(size_t i=0;i<orphan;i++){hash^=triangles[i].material;hash*=UINT64_C(1099511628211);}
    hash^=orphan;
    int ok=rocket_runtime_platforms(platforms,groups);
    if(ok&&meshHashes[1]!=hash){ok=rocket_runtime_mesh(1,triangles,orphan);if(ok)meshHashes[1]=hash;}
    free(platforms);return ok;
}
static int sync_mesh(struct MarioState *m,int dynamic) {
    SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
    size_t capacity=(size_t)(gSurfaceNodesAllocated>0?gSurfaceNodesAllocated:1),n=0;
    if(capacity>1000000)return 0;
    struct Surface **surfaces=malloc(capacity*sizeof(*surfaces));
    if(!surfaces)return 0;
    for(int x=0;x<NUM_CELLS;++x)for(int z=0;z<NUM_CELLS;++z)for(int p=0;p<3;++p)
        for(struct SurfaceNode *node=partition[x][z][p].next;node;node=node->next) {
            if(!solid_surface(node->surface)||(phaseActive&&native_phase_surface(node->surface)))continue;
            if(n==capacity){free(surfaces);return 0;}surfaces[n++]=node->surface;
        }
    qsort(surfaces,n,sizeof(*surfaces),pointer_compare);
    size_t unique=0;struct Surface *previous=NULL;
    for(size_t i=0;i<n;++i)if(surfaces[i]!=previous){previous=surfaces[i];surfaces[unique++]=previous;}
    if(unique>100000){free(surfaces);return 0;}
    RocketTriangle *triangles=malloc((unique?unique:1)*sizeof(*triangles));
    if(!triangles){free(surfaces);return 0;}
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<unique;++i) {
        triangles[i].material=rocket_environment_material(m,surfaces[i]);
        hash^=triangles[i].material;hash*=UINT64_C(1099511628211);
        const s16 *vertices[]={surfaces[i]->vertex1,surfaces[i]->vertex2,surfaces[i]->vertex3};
        for(int v=0;v<3;++v)for(int k=0;k<3;++k){
            triangles[i].v[v][k]=vertices[v][k];
            hash^=(uint16_t)vertices[v][k];hash*=UINT64_C(1099511628211);
        }
    }
    hash^=unique;int ok=1;
    if(dynamic)ok=sync_platforms(surfaces,triangles,unique);
    else if(meshHashes[dynamic]!=hash) {ok=rocket_runtime_mesh(dynamic,triangles,unique);if(ok)meshHashes[dynamic]=hash;}
    free(triangles);free(surfaces);return ok;
}
void rocket_adapter_suspend(void) {
    // A handoff at the current pose must not strand native Mario inside a grate.
    // Never move a player after an external warp, area change, or level change.
    if(player&&player->area==area&&level==gCurrLevelNum){
        float moved=0;for(int k=0;k<3;k++){float d=player->pos[k]-lastPosition[k];moved+=d*d;}
        RocketSnapshot pose,recovered;
        if(phaseActive&&moved<1.f&&rocket_runtime_snapshot(&pose)&&phase_overlap(&pose,0)&&phase_clear_pose(&pose,&recovered)){
            vec3f_copy(player->pos,recovered.position);vec3f_set(player->vel,0,0,0);
            player->forwardVel=player->slideVelX=player->slideVelZ=0;
            if(player->marioObj)vec3f_copy(player->marioObj->header.gfx.pos,player->pos);
        }
    }
    if(ownHide&&player&&player->marioObj)player->marioObj->header.gfx.node.flags&=~GRAPH_RENDER_INVISIBLE;
    ownHide=0;player=NULL;area=NULL;haveFrame=0;frame=0;meshHashes[0]=meshHashes[1]=0;
    phaseActive=haveClearPose=0;
    textJumpHeld=1;textJumpPressed=textHaveFrame=0;
    memset(platformIdentities,0,sizeof(platformIdentities));
    rocket_runtime_suspend();
}
void rocket_adapter_set_selected(int active) {
    active = !!active;
    if (selected != active) rocket_adapter_suspend();
    selected = active;
}
int rocket_adapter_car_selected(void) { return selected && rocket_runtime_enabled(); }
const char *rocket_adapter_switch_reason(void) {
    RocketSnapshot state;
    if (!selected || !player || !rocket_runtime_snapshot(&state)) return "Wait for the car to settle";
    if (phaseActive&&phase_overlap(&state,0)) return "Clear the Vanish barrier before switching";
    if (!state.grounded || state.flipping || !isfinite(state.basis[7]) || state.basis[7] < .98f)
        return "Park upright on all four wheels";
    for (int i = 0; i < 4; ++i) if (!state.wheel_contacts[i]) return "Park upright on all four wheels";
    for (int i = 0; i < 3; ++i)
        if (!isfinite(state.velocity[i]) || !isfinite(state.angular_velocity[i]) ||
            fabsf(state.velocity[i]) > 60.f || fabsf(state.angular_velocity[i]) > .2f)
            return "Stop the car before switching";
    return NULL;
}
static int supported(u32 action) {
    switch(action) {
        case ACT_IDLE:case ACT_WALKING:case ACT_DECELERATING:case ACT_BRAKING:case ACT_BRAKING_STOP:
        case ACT_TURNING_AROUND:case ACT_FINISH_TURNING_AROUND:case ACT_FREEFALL:case ACT_FREEFALL_LAND:return 1;
        case ACT_WATER_IDLE:case ACT_WATER_PLUNGE:case ACT_WATER_ACTION_END:
        case ACT_BREASTSTROKE:case ACT_SWIMMING_END:case ACT_FLUTTER_KICK:
        case ACT_METAL_WATER_STANDING:case ACT_METAL_WATER_WALKING:case ACT_METAL_WATER_FALLING:
        case ACT_METAL_WATER_FALL_LAND:case ACT_METAL_WATER_JUMP:case ACT_METAL_WATER_JUMP_LAND:return 1;
        default:return 0;
    }
}
int rocket_adapter_platform_contact(struct MarioState *m,struct Surface *floor,float height) {
    RocketSnapshot state;
    if(m!=player||!floor||!floor->object||floor->normal.y<.5f||!isfinite(height)||
       !rocket_adapter_interaction_snapshot(&state)||!state.grounded||state.basis[7]<.5f)return 0;
    struct Object *object=floor->object;
    if(!obj_has_behavior(object,bhvCapSwitch)&&!obj_has_behavior(object,bhvFloorSwitchGrills)&&
       !obj_has_behavior(object,bhvFloorSwitchHardcodedModel)&&!obj_has_behavior(object,bhvFloorSwitchAnimatesObject))return 0;
    /* Native Mario stands at his feet; Octane mirrors its chassis origin.
     * Require real suspension contact and a floor directly below that origin.
     * This only supplies platform identity; switch behavior owns progression. */
    float clearance=state.position[1]-height;
    if(clearance<0||clearance>70.f)return 0;
    for(int i=0;i<4;i++)if(state.wheel_contacts[i]&&
        fabsf(state.wheel_position[i][1]-state.wheel_radius[i]-height)<12.f)return 1;
    return 0;
}
static RocketInput keyboard_input(const struct MarioState *m) {
    float x=-m->controller->rawStickX/80.f,y=m->controller->rawStickY/80.f;
    u16 buttons=m->controller->buttonDown;
    RocketInput input={0};input.throttle=y;input.steer=x;input.pitch=-y;
    if(buttons&Z_TRIG)input.roll=x;else input.yaw=x;
    input.jump=!!(buttons&A_BUTTON);input.boost=!!(buttons&B_BUTTON);input.powerslide=!!(buttons&Z_TRIG);
    return input;
}
static float door_ray_distance(const struct Surface *surface,const Vec3f from,const Vec3f dir,float length) {
    float e1[3],e2[3],offset[3],h[3],q[3];
    for(int k=0;k<3;++k){e1[k]=surface->vertex2[k]-surface->vertex1[k];e2[k]=surface->vertex3[k]-surface->vertex1[k];offset[k]=from[k]-surface->vertex1[k];}
    for(int k=0;k<3;++k){int a=(k+1)%3,b=(k+2)%3;h[k]=dir[a]*e2[b]-dir[b]*e2[a];q[k]=offset[a]*e1[b]-offset[b]*e1[a];}
    float det=0,u=0,v=0,t=0;
    for(int k=0;k<3;++k){det+=e1[k]*h[k];u+=offset[k]*h[k];v+=dir[k]*q[k];t+=e2[k]*q[k];}
    if(fabsf(det)<.00001f)return FLT_MAX;
    u/=det;v/=det;t/=det;
    return u>=0&&v>=0&&u+v<=1&&t>=0&&t<=length?t:FLT_MAX;
}
static int cap_object_eligible(struct Object *object) {
    return object&&(object->activeFlags&ACTIVE_FLAG_ACTIVE)&&area&&
        object->header.gfx.activeAreaIndex==area->index;
}
int rocket_adapter_vanish_switch_contact(struct Object *object) {
    RocketSnapshot pose;
    if(!cap_object_eligible(object)||object->behavior!=bhvCapSwitch||object->oBehParams2ndByte!=2||
       object->oAction!=1||!rocket_adapter_interaction_snapshot(&pose)||!pose.grounded)return 0;
    // Only an actual contacting tire on this switch's top can press it. A blue
    // exclamation box and a side/underside bumper contact are not unlock switches.
    for(int x=0;x<NUM_CELLS;x++)for(int z=0;z<NUM_CELLS;z++)
        for(struct SurfaceNode *node=gDynamicSurfacePartition[x][z][0].next;node;node=node->next){
            struct Surface *s=node->surface;
            if(s->object!=object||!solid_surface(s)||s->normal.y<=.01f)continue;
            for(int i=0;i<4;i++)if(pose.wheel_contacts[i]){
                Vec3f down={0,-1,0};
                float distance=door_ray_distance(s,pose.wheel_position[i],down,pose.wheel_radius[i]+8.f);
                if(distance>=pose.wheel_radius[i]-8.f&&distance<=pose.wheel_radius[i]+8.f)return 1;
            }
        }
    return 0;
}
static int cap_actor_object_eligible(struct Object *object) {
    return object&&(object->activeFlags&ACTIVE_FLAG_ACTIVE)&&gCurrentArea&&
        object->header.gfx.activeAreaIndex==gCurrentArea->index;
}
static int object_visible_with_phase(const float from[3],struct Object *object,int phase);
int rocket_adapter_pickup_pose(RocketSnapshot *pose) {
    if(!selected)return 0;
    if(!player||!haveFrame||(u32)(gGlobalTimer-previousFrame)>1||
       !rocket_adapter_interaction_snapshot(pose))return -1;
    return 1;
}
int rocket_adapter_cap_pickup_contact(const RocketSnapshot *pose,struct Object *object,unsigned caps) {
    if(!cap_actor_object_eligible(object)||object->oIntangibleTimer||
       object->oInteractType!=INTERACT_CAP||(object->oInteractStatus&INT_STATUS_INTERACTED)||
       (object->behavior!=bhvWingCap&&object->behavior!=bhvMetalCap&&object->behavior!=bhvVanishCap)||
       !isfinite(object->hitboxHeight)||object->hitboxHeight<=0)return 0;
    const float bottom[3]={object->oPosX,object->oPosY-object->hitboxDownOffset,object->oPosZ};
    return rocket_body_overlaps_cylinder(pose,bottom,object->hitboxRadius,object->hitboxHeight)&&
        object_visible_with_phase(pose->position,object,!!(caps&MARIO_VANISH_CAP));
}
static int cap_box_pose_contact(const RocketSnapshot *pose,struct Object *object,unsigned caps,const RocketInput *input) {
    if(!rocket_body_pose_valid(pose)||!cap_actor_object_eligible(object)||
       object->behavior!=bhvExclamationBox||object->oBehParams2ndByte>2||
       object->oAction!=2||object->oIntangibleTimer||object->oExclamationBoxForce||
       !object_visible_with_phase(pose->position,object,!!(caps&MARIO_VANISH_CAP)))return 0;
    for(int x=0;x<NUM_CELLS;x++)for(int z=0;z<NUM_CELLS;z++)for(int p=0;p<3;p++)
        for(struct SurfaceNode *node=gDynamicSurfacePartition[x][z][p].next;node;node=node->next){
            struct Surface *surface=node->surface;
            if(surface->object!=object||!solid_surface(surface))continue;
            float triangle[3][3];const s16 *v[]={surface->vertex1,surface->vertex2,surface->vertex3};
            for(int i=0;i<3;i++)for(int k=0;k<3;k++)triangle[i][k]=v[i][k];
            if(!rocket_car_triangle_overlap(pose,triangle,8.f))continue;
            if(!input)return 1;
            float toward=pose->basis[0]*surface->normal.x+pose->basis[2]*surface->normal.z;
            if((input->throttle>.2f&&toward<-.5f)||(input->throttle<-.2f&&toward>.5f)||
               (input->jump&&surface->normal.y<-.5f)||pose->velocity[1]*surface->normal.y<-60.f)return 1;
        }
    return 0;
}
int rocket_adapter_cap_box_pose_contact(const RocketSnapshot *pose,struct Object *object,unsigned caps) {
    return cap_box_pose_contact(pose,object,caps,NULL);
}
int rocket_adapter_cap_box_contact(struct Object *object) {
    RocketSnapshot pose;RocketInput keyboard,input;
    if(rocket_adapter_pickup_pose(&pose)!=1)return 0;
    keyboard=keyboard_input(player);
    return rocket_runtime_read_input(&keyboard,&input)&&
        cap_box_pose_contact(&pose,object,rocket_caps_active_flags(0),&input);
}
/* Keep the original caller ABI while all native cap boxes share the hook. */
int rocket_adapter_vanish_box_contact(struct Object *object) {
    return rocket_adapter_cap_box_contact(object);
}
static void cap_pickups(struct MarioState *m) {
    RocketSnapshot pose;
    if(!gObjectLists||!rocket_adapter_interaction_snapshot(&pose))return;
    for(int list=0;list<NUM_OBJ_LISTS;list++){
        struct ObjectNode *head=&gObjectLists[list];
        for(struct ObjectNode *node=head->next;node&&node!=head;node=node->next){
            struct Object *o=(struct Object*)node;
            int pickup=o->oInteractType==INTERACT_CAP&&(o->behavior==bhvWingCap||o->behavior==bhvMetalCap||o->behavior==bhvVanishCap);
            int star=o->oInteractType==INTERACT_STAR_OR_KEY;
            if((!pickup&&!star)||!cap_object_eligible(o)||o->oIntangibleTimer||
               (o->oInteractStatus&INT_STATUS_INTERACTED)||o->hitboxRadius<=0||o->hitboxHeight<=0)continue;
            if(pickup){
                if(!rocket_adapter_cap_pickup_contact(&pose,o,rocket_caps_active_flags(0)))continue;
            }else{
            // Intersect the native upright pickup cylinder with the complete
            // chassis using its horizontal cross-section at the pickup center.
            // The native handler still owns collection, cap timing and stars.
            float center[3]={o->oPosX,o->oPosY-o->hitboxDownOffset+o->hitboxHeight*.5f,o->oPosZ};
            const float half[3]={120.507f*.5f*ROCKET_HOST_SCALE,86.6994f*.5f*ROCKET_HOST_SCALE,38.6591f*.5f*ROCKET_HOST_SCALE};
            const float offset[3]={13.8757f*ROCKET_HOST_SCALE,0,20.755f*ROCKET_HOST_SCALE};
            float nearest[3];vec3f_copy(nearest,pose.position);
            for(int a=0;a<3;a++){
                float local=0;for(int k=0;k<3;k++)local+=(center[k]-pose.position[k])*pose.basis[a*3+k];
                local=fmaxf(offset[a]-half[a],fminf(offset[a]+half[a],local));
                for(int k=0;k<3;k++)nearest[k]+=pose.basis[a*3+k]*local;
            }
            float dx=nearest[0]-center[0],dz=nearest[2]-center[2];
            if(fabsf(nearest[1]-center[1])>o->hitboxHeight*.5f||dx*dx+dz*dz>o->hitboxRadius*o->hitboxRadius||
               !rocket_adapter_object_visible(pose.position,o))continue;
            }
            int i=0;for(;i<m->marioObj->numCollidedObjs;i++)if(m->marioObj->collidedObjs[i]==o)break;
            if(i==m->marioObj->numCollidedObjs){
                if(i>=4)continue;
                m->marioObj->collidedObjs[i]=o;m->marioObj->numCollidedObjs++;
            }
            m->marioObj->collidedObjInteractTypes|=o->oInteractType;
            m->collidedObjInteractTypes|=o->oInteractType;
        }
    }
}
static int object_visible_along(const float from[3],struct Object *object,
        const float direction[3],float length,int phase) {
    float nearest=FLT_MAX;struct Surface *hit=NULL;
    // Filter before choosing intersections, including coincident triangles.
    // Scan both complete host partitions so short rays across a cell boundary
    // cannot miss a wall. Callers first check nearby reach or physical contact.
    for(int dynamic=0;dynamic<2;++dynamic){
        SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
        for(int x=0;x<NUM_CELLS;++x)for(int z=0;z<NUM_CELLS;++z)for(int p=0;p<3;++p)
            for(struct SurfaceNode *node=partition[x][z][p].next;node;node=node->next){
                struct Surface *surface=node->surface;if(!solid_surface(surface)||(phase&&native_phase_surface(surface)))continue;
                float distance=door_ray_distance(surface,from,direction,length);
                if(distance<nearest||(distance!=FLT_MAX&&distance==nearest&&surface->object!=object)){nearest=distance;hit=surface;}
            }
    }
    return !hit||hit->object==object;
}
static int object_visible_with_phase(const float from[3],struct Object *door,int phase) {
    Vec3f direction={door->oPosX-from[0],0,door->oPosZ-from[2]};
    float length=hypotf(direction[0],direction[2]);if(length<.001f)return 1;
    direction[0]/=length;direction[2]/=length;
    return object_visible_along(from,door,direction,length,phase);
}
int rocket_adapter_whomp_path_clear(const float from[3],const float to[3],struct Object *object) {
    float direction[3],length=0;
    for(int k=0;k<3;k++){direction[k]=to[k]-from[k];length+=direction[k]*direction[k];}
    length=sqrtf(length);if(!isfinite(length)||length<.001f)return 0;
    for(int k=0;k<3;k++)direction[k]/=length;
    /* No cap permission is inferred from a remote pose. */
    return object_visible_along(from,object,direction,length,0);
}
int rocket_adapter_object_visible(const float from[3],struct Object *object) {
    return object_visible_with_phase(from,object,phaseActive);
}
int rocket_adapter_enemy_visible(const float from[3],struct Object *object,unsigned verifiedCaps) {
    if(!from||!object||!isfinite(object->hitboxHeight)||object->hitboxHeight<=0||
       !isfinite(object->hitboxDownOffset))return 0;
    /* A horizontal door-reach ray cuts through the supporting floor when an
     * enemy stands uphill. Trace to its actual cylinder center instead; floors,
     * ceilings and walls still obstruct the complete three-dimensional ray. */
    Vec3f direction={object->oPosX-from[0],
        object->oPosY-object->hitboxDownOffset+object->hitboxHeight*.5f-from[1],
        object->oPosZ-from[2]};
    for(int k=0;k<3;k++)if(!isfinite(from[k])||!isfinite(direction[k]))return 0;
    float length=sqrtf(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
    if(!isfinite(length))return 0;
    if(length<.001f)return 1;
    for(int k=0;k<3;k++)direction[k]/=length;
    return object_visible_along(from,object,direction,length,!!(verifiedCaps&MARIO_VANISH_CAP));
}
#include "rocket_ccm_chimney.inc.h"
#include "rocket_jrb_entry.inc.h"
#include "rocket_pss_entry.inc.h"
#include "rocket_pipe_entry.inc.h"
/* Read the already-remapped car jump without forwarding it to unrelated
 * native actions. No target consumes it here: ordinary jumping stays intact. */
static void prepare_text_input(struct MarioState *m) {
    if(!m||m->playerIndex)return;
    if(textHaveFrame&&textInputFrame==gGlobalTimer)return;
    textInputFrame=gGlobalTimer;textHaveFrame=1;textJumpPressed=0;
    if(!selected||m!=player||!m->controller){textJumpHeld=1;return;}
    RocketInput keyboard=keyboard_input(m),input;
    if(!rocket_runtime_read_input(&keyboard,&input)){textJumpHeld=1;return;}
    textJumpPressed=input.jump&&!textJumpHeld;
    textJumpHeld=!!input.jump;
}
int rocket_adapter_text_pressed(struct MarioState *m,struct Object *o) {
    RocketSnapshot state;
    if(!m||m->playerIndex||m!=player||!m->area||!m->marioObj||!o||!textHaveFrame||textInputFrame!=gGlobalTimer||!textJumpPressed||
       !(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->header.gfx.activeAreaIndex!=m->area->index||
       o->oIntangibleTimer||!(o->oInteractType&INTERACT_TEXT)||
       (o->oInteractStatus&INT_STATUS_INTERACTED)||
       (gNetworkType!=NT_NONE&&(!gNetworkAreaLoaded||gNetworkAreaSyncing||!gNetworkPlayerLocal))||
       !rocket_adapter_interaction_snapshot(&state)||!state.grounded||state.flipping||!isfinite(state.basis[7])||state.basis[7]<.75f)return 0;
    /* Keep native range/collision arbitration. Do not scan for or extend reach
     * to NPCs/signs, and never borrow another player's input or remote pose. */
    int collided=0;
    for(int i=0;i<m->marioObj->numCollidedObjs&&i<4;i++)if(m->marioObj->collidedObjs[i]==o)collided=1;
    return collided&&rocket_adapter_enemy_visible(state.position,o,0);
}
#include "rocket_whomp_crush.inc.h"
void rocket_adapter_prepare_interactions(struct MarioState *m) {
    if(whomp_crush_prepare(m))return;
    prepare_text_input(m);
    if(!selected||!m||m!=player||!m->marioObj||!m->controller||!m->area||m->area!=area||
       level!=gCurrLevelNum||!supported(m->action)||m->health<0x100||m->heldObj||
       m->riddenObj||m->heldByObj||m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED||
       0||!gObjectLists)return;
    cap_pickups(m);
    if(rocket_pipe_interaction(m)||ccm_chimney_interaction(m)||jrb_ship_interaction(m)||pss_alcove_interaction(m))return;
    RocketSnapshot state;RocketInput keyboard=keyboard_input(m),input;
    if(!rocket_runtime_read_input(&keyboard,&input)||input.throttle<=.2f||input.jump||
       !rocket_runtime_snapshot(&state)||!state.grounded||state.basis[7]<.75f)return;
    float moved=0;for(int k=0;k<3;++k){float d=m->pos[k]-state.position[k];moved+=d*d;}
    if(moved>500.f*500.f)return; // External warp must be processed before new interaction.
    // Front extent of the pinned Octane hitbox (half length + forward offset).
    // This only extends door reach; all other host interaction shapes stay intact.
    const float front=(120.507f*.5f+13.8757f)*ROCKET_HOST_SCALE;
    float length=hypotf(state.basis[0],state.basis[2]);if(length<.5f)return;
    float fx=state.basis[0]/length,fz=state.basis[2]/length;
    Vec3f origin={m->pos[0],m->pos[1]+40.f,m->pos[2]};
    struct Object *nearest=NULL;float nearestDistance=1e9f;
    struct ObjectNode *head=&gObjectLists[OBJ_LIST_SURFACE];
    for(struct ObjectNode *node=head->next;node&&node!=head;node=node->next) {
        struct Object *door=(struct Object *)node;
        u32 type=door->oInteractType;
        if((type!=INTERACT_DOOR&&type!=INTERACT_WARP_DOOR)||
           !(door->activeFlags&ACTIVE_FLAG_ACTIVE)||door->header.gfx.activeAreaIndex!=m->area->index||
           door->oIntangibleTimer||door->oAction||door->hitboxRadius<=0||
           (door->oInteractStatus&INT_STATUS_INTERACTED))continue;
        float bottom=door->oPosY-door->hitboxDownOffset;
        if(origin[1]<bottom||origin[1]>bottom+door->hitboxHeight)continue;
        float dx=door->oPosX-origin[0],dz=door->oPosZ-origin[2];
        float ahead=dx*fx+dz*fz;if(ahead<=0)continue;
        float reach=fminf(ahead,front),x=dx-fx*reach,z=dz-fz*reach;
        float radius=door->hitboxRadius+20.f,distance=dx*dx+dz*dz;
        if(x*x+z*z>radius*radius||distance>=nearestDistance)continue;
        if(!rocket_adapter_object_visible(origin,door))continue;
        nearest=door;nearestDistance=distance;
    }
    if(!nearest)return;
    int index=0;for(;index<m->marioObj->numCollidedObjs;++index)
        if(m->marioObj->collidedObjs[index]==nearest)break;
    if(index==m->marioObj->numCollidedObjs) {
        if(index>=4)return;
        m->marioObj->collidedObjs[index]=nearest;m->marioObj->numCollidedObjs++;
    }
    m->marioObj->collidedObjInteractTypes|=nearest->oInteractType;
    m->collidedObjInteractTypes|=nearest->oInteractType;
    // Native handlers require WALKING/DECELERATING. They still own every key,
    // star requirement, dialog, door animation and warp; no door is forced open.
    m->action=ACT_WALKING;
}
int rocket_adapter_platform_snapshot(RocketSnapshot *state) {
    return player&&rocket_adapter_body_snapshot(player->marioObj,state);
}
int rocket_adapter_body_snapshot(struct Object *object,RocketSnapshot *state) {
    struct MarioState *m=player;
    if(!state||!object||!selected||!m||m->playerIndex!=0||m->marioObj!=object||
       !m->area||m->area!=area||level!=gCurrLevelNum||!haveFrame||
       (u32)(gGlobalTimer-previousFrame)>1||!supported(m->action)||m->health<0x100||
       m->heldObj||m->heldByObj||m->riddenObj||m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED||
       (!rocket_runtime_rule_ready()||!rocket_runtime_snapshot(state)))return 0;
    float distance=0;
    for(int k=0;k<3;k++){float d=m->pos[k]-state->position[k];distance+=d*d;}
    return distance<=500.f*500.f;
}
int rocket_adapter_interaction_snapshot(RocketSnapshot *state) {
    if(!rocket_adapter_platform_snapshot(state)||0)return 0;
    RocketInput keyboard=keyboard_input(player),input;
    return rocket_runtime_read_input(&keyboard,&input);
}
int rocket_adapter_update(struct MarioState *m) {
    if(!selected||!m||m->playerIndex!=0)return 0;
    if(!rocket_runtime_enabled()||!m->marioObj||!m->controller||!m->area) {rocket_adapter_suspend();return 0;}
    if(player&&(player!=m||area!=m->area||level!=gCurrLevelNum))rocket_adapter_suspend();
    int metal=!!(m->flags&MARIO_METAL_CAP);
    int submerged=m->pos[1]<m->waterLevel-100.f;
    /* A cap can be collected while swimming, or native shock can recover into
     * water idle. Reacquire only ordinary swimming, never drowning/whirlpool. */
    int metalEntry=metal&&submerged&&(m->action==ACT_WATER_IDLE||m->action==ACT_WATER_PLUNGE||
        m->action==ACT_BREASTSTROKE||m->action==ACT_SWIMMING_END||m->action==ACT_FLUTTER_KICK);
    if((whompCrush.m==m&&m->squishTimer>0&&m->squishTimer<255)||(!supported(m->action)&&!metalEntry)||m->health<0x100||m->heldObj||m->riddenObj||m->heldByObj||m->quicksandDepth>1||(m->input&INPUT_SQUISHED)) {
        rocket_adapter_suspend();return 0;
    }
    if(player) {
        float x=m->pos[0]-lastPosition[0],y=m->pos[1]-lastPosition[1],z=m->pos[2]-lastPosition[2];
        // This observes external teleports before our step, not high car speed.
        if(x*x+y*y+z*z>500.f*500.f)rocket_adapter_suspend();
    }
    if(!player) {
        // Keep the host visible until an unfrozen frame can submit a car pose.
        if(m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED)return 0;
        float p[3]={m->pos[0],m->pos[1]+40.f,m->pos[2]},v[3]={0,0,0};
        phaseActive=!!(m->flags&MARIO_VANISH_CAP);
        if(m->pos[1]<m->waterLevel-100.f)p[1]=m->pos[1];
        if(m->action==ACT_FREEFALL||metalEntry||(m->action&ACT_FLAG_METAL_WATER))for(int i=0;i<3;++i)v[i]=m->vel[i]*30.f;
        if(!sync_mesh(m,0)||!sync_mesh(m,1)||!rocket_runtime_reset(p,v,(float)(u16)m->faceAngle[1]*(6.28318530718f/65536.f))) {
            rocket_adapter_suspend();return 0;
        }
        player=m;area=m->area;level=gCurrLevelNum;
    }
    if(m->freeze||sCurrPlayMode==PLAY_MODE_PAUSED){rocket_runtime_interrupt();m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;ownHide=1;return 1;}
    if(!haveFrame||previousFrame!=gGlobalTimer) {
        int nextPhase=!!(m->flags&MARIO_VANISH_CAP);
        RocketSnapshot pose,recovered;
        if(phaseActive&&!nextPhase&&rocket_runtime_snapshot(&pose)&&phase_overlap(&pose,0)){
            if(!phase_clear_pose(&pose,&recovered)||!rocket_runtime_recover(&recovered)){rocket_adapter_suspend();return 0;}
        }
        phaseActive=nextPhase;
        if(!sync_mesh(m,0)||!sync_mesh(m,1)){rocket_adapter_suspend();return 0;}
        RocketSnapshot before={0};RocketEnvironment environment={0};
        if(rocket_runtime_snapshot(&before)){
            /* Recovery may move the chassis across a floor or water boundary.
             * Rebind only the sampling copy; native progression and cap timers
             * remain owned by Mario, and clearPose supplies geometry only. */
            struct MarioState environmentProbe=*m;
            vec3f_copy(environmentProbe.pos,before.position);
            environmentProbe.floorHeight=find_floor(before.position[0],before.position[1],
                before.position[2],&environmentProbe.floor);
            environmentProbe.waterLevel=find_water_level(before.position[0],before.position[2]);
            int mode=rocket_water_classify(before.water_mode,
                environmentProbe.waterLevel>gLevelValues.floorLowerLimit,
                environmentProbe.waterLevel,before.position[1],!!(m->flags&MARIO_METAL_CAP));
            environmentProbe.action=mode==ROCKET_WATER_JET?ACT_WATER_IDLE:
                mode==ROCKET_WATER_METAL?(before.grounded?ACT_METAL_WATER_STANDING:ACT_METAL_WATER_FALLING):
                before.grounded?ACT_IDLE:ACT_FREEFALL;
            rocket_environment_sample(&environmentProbe,&before,&environment);
        }
        if(!rocket_runtime_set_environment(&environment)){rocket_adapter_suspend();return 0;}
        RocketInput input=keyboard_input(m);
        rocket_runtime_set_water(m->waterLevel>gLevelValues.floorLowerLimit,m->waterLevel,!!(m->flags&MARIO_METAL_CAP));
        rocket_runtime_set_water_query(native_water);
        /* The environment bridge owns native currents; jet damping uses zero flow. */
        if(rocket_runtime_frame(++frame,&input,0,0)<0){rocket_adapter_suspend();return 0;}
        previousFrame=gGlobalTimer;haveFrame=1;
    }
    RocketSnapshot state;if(!rocket_runtime_snapshot(&state)){rocket_adapter_suspend();return 0;}
    if(!phase_overlap(&state,0)){clearPose=state;haveClearPose=1;}
    vec3f_copy(m->pos,state.position);
    for(int i=0;i<3;++i)m->vel[i]=state.velocity[i]/30.f;
    // A somersault crosses vertical and inverts projected forward. Keep the
    // host camera heading stable through it; the rendered car uses full basis.
    if(state.grounded||(!state.flipping&&state.basis[7]>.5f)) {
        // SM64's atan2f is not the standard Cartesian function.
        float heading=(float)atan2((double)state.basis[0],(double)state.basis[2]);if(heading<0)heading+=6.28318530718f;
        m->faceAngle[1]=(s16)(u16)(heading*(65536.f/6.28318530718f));
    }
    // Resample after motion too: crossing a water-box edge must clear the HUD
    // and native swimming action immediately, without a reset/refill/plunge.
    m->waterLevel=find_water_level(m->pos[0],m->pos[2]);
    int water=rocket_water_classify(state.water_mode,m->waterLevel>gLevelValues.floorLowerLimit,
        m->waterLevel,m->pos[1],!!(m->flags&MARIO_METAL_CAP));
    rocket_runtime_set_water(m->waterLevel>gLevelValues.floorLowerLimit,m->waterLevel,!!(m->flags&MARIO_METAL_CAP));
    m->prevAction=m->action;
    if(water==ROCKET_WATER_JET)m->action=ACT_WATER_IDLE; // preserves native breath/health tail
    else if(water==ROCKET_WATER_METAL)m->action=state.grounded?(hypotf(state.velocity[0],state.velocity[2])>30.f?ACT_METAL_WATER_WALKING:ACT_METAL_WATER_STANDING):ACT_METAL_WATER_FALLING;
    else m->action=state.grounded?ACT_IDLE:ACT_FREEFALL;
    m->forwardVel=hypotf(m->vel[0],m->vel[2]);m->slideVelX=m->vel[0];m->slideVelZ=m->vel[2];
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);vec3s_set(m->marioObj->header.gfx.angle,0,m->faceAngle[1],0);
    m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;ownHide=1;
    vec3f_copy(lastPosition,m->pos);
    return 1;
}
