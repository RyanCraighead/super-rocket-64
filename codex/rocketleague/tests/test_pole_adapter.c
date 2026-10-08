/* Actual adapter and pole bridge; native action dispatch is an explicit
 * boundary here and is tested verbatim in test_pole.c. */
#define ROCKET_POLE_REAL_TEST
#define main adapter_fixture_main
#include "test_adapter.c"
#undef main
#include "game/rocket_pole.h"
#include "engine/math_util.h"
#include "../physics/pole_pose.h"
const BehaviorScript bhvPoleGrabbing[]={44};
static struct Object poleObject;
int rocket_runtime_read_selected_input(const RocketInput*k,RocketInput*out){*out=rocket_gamepad_merge(k,&fixtureGamepad);return !uiBlocked&&enabled;}
s32 mario_execute_automatic_action(struct MarioState*m){
    if(m->input&INPUT_A_PRESSED){m->prevAction=m->action;m->action=ACT_WALL_KICK_AIR;m->vel[1]=62;m->forwardVel=24;m->faceAngle[1]+=0x8000;return 1;}
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);return 0;
}
int main(void){
    fresh();step();gCurrLevelNum=LEVEL_SSL;testArea.index=2;step();
    memset(&poleObject,0,sizeof poleObject);poleObject.behavior=bhvPoleGrabbing;
    poleObject.activeFlags=ACTIVE_FLAG_ACTIVE;poleObject.header.gfx.activeAreaIndex=2;poleObject.hitboxHeight=920;poleObject.hitboxRadius=80;
    pose.position[1]=mario.pos[1]=400;pose.grounded=0;pose.boost=23;object.hitboxRadius=37;object.hitboxHeight=160;
    mario.action=ACT_FREEFALL;assert(rocket_pole_can_grab(&mario,&poleObject));
    poleObject.oInteractType=INTERACT_POLE;
    struct ObjectNode *head=&gObjectLists[OBJ_LIST_POLELIKE];head->next=(struct ObjectNode*)&poleObject;
    poleObject.header.next=head;poleObject.header.prev=head;
    // Actual full-car reach catches the nose before Mario's smaller cylinder
    // could reach the pole. Do not enlarge native reach for other objects.
    pose.position[2]=mario.pos[2]=-180;rocket_adapter_prepare_interactions(&mario);
    assert(object.numCollidedObjs==1&&object.collidedObjs[0]==&poleObject&&(mario.collidedObjInteractTypes&INTERACT_POLE));
    rocket_adapter_prepare_interactions(&mario);assert(object.numCollidedObjs==1);
    object.numCollidedObjs=0;pose.position[2]=mario.pos[2]=-250;rocket_adapter_prepare_interactions(&mario);assert(!object.numCollidedObjs);
    pose.position[2]=mario.pos[2]=0;
    mario.usedObj=&poleObject;mario.action=ACT_HOLDING_POLE;object.oMarioPolePos=400;
    gGlobalTimer++;assert(!rocket_adapter_update(&mario)&&!draw);
    assert(rocket_pole_execute(&mario)==0);int before=resets;
    controller.buttonDown=A_BUTTON;gGlobalTimer++;assert(rocket_pole_execute(&mario)==0&&mario.action==ACT_WALL_KICK_AIR);
    mario.freeze=1;gGlobalTimer++;assert(!rocket_adapter_update(&mario)&&resets==before);
    mario.freeze=0;step();assert(resets==before+1&&draw&&pose.basis[1]==1&&pose.velocity[1]==1860);
    assert(pose.boost==23&&pose.jumped&&mario.action==ACT_FREEFALL);
    assert(!rocket_pole_can_grab(&mario,&poleObject)); // no instant regrab before separation
    for(int i=0;i<3;i++)step();assert(resets==before+1);
    pose.position[2]=mario.pos[2]=200;step();pose.position[2]=mario.pos[2]=0;step();
    assert(rocket_pole_can_grab(&mario,&poleObject));
    for(int dynamic=0;dynamic<2;dynamic++)for(int x=-1025;x<=1025;x+=1025){
        RocketSnapshot box=pose;float at[3]={(float)x,400,0};rocket_pole_pose(&box,at,0,1);
        struct Surface wall={0};wall.normal.x=-1;
        vec3s_set(wall.vertex1,x+80,300,-500);vec3s_set(wall.vertex2,x+80,900,500);vec3s_set(wall.vertex3,x+80,900,-500);
        struct SurfaceNode node={.surface=&wall};int cx=(x+80+LEVEL_BOUNDARY_MAX)/CELL_SIZE,cz=LEVEL_BOUNDARY_MAX/CELL_SIZE;
        SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
        struct SurfaceNode *head=&partition[cz][cx][SPATIAL_PARTITION_WALLS],*old=head->next;head->next=&node;
        assert(!rocket_adapter_pole_pose_clear(&box,0));wall.flags=SURFACE_FLAG_INTANGIBLE;
        assert(rocket_adapter_pole_pose_clear(&box,0));wall.flags=0;wall.type=SURFACE_VANISH_CAP_WALLS;
        assert(!rocket_adapter_pole_pose_clear(&box,0)&&rocket_adapter_pole_pose_clear(&box,MARIO_VANISH_CAP));
        head->next=old;
    }
    puts("PASS actual pole/adapter handoff: native ownership, freeze, vertical velocity/pose, retained fuel, one reset, same-pole rearm after exit");
}
