/* Standard-pipe rim contact, scoped to the reproduced native Bowser/THI
 * routes. The actual pipe mesh remains solid; native warp dispatch owns the
 * transition, destination, hooks and re-entry cooldown. */
#include "actors/common1.h"
static int rocket_pipe_route(const struct ObjectWarpNode *node,int areaIndex) {
    if(!node)return 0;
    const struct WarpNode *w=&node->node;
    if(gCurrLevelNum==LEVEL_BITDW||gCurrLevelNum==LEVEL_BITS)
        return areaIndex==1&&w->id==0x0b&&w->destArea==1&&w->destNode==0x0a&&
            (w->destLevel&0x7f)==(gCurrLevelNum==LEVEL_BITDW?LEVEL_BOWSER_1:LEVEL_BOWSER_3);
    return gCurrLevelNum==LEVEL_THI&&(areaIndex==1||areaIndex==2)&&w->id>=0x32&&w->id<=0x34&&
        (w->destLevel&0x7f)==LEVEL_THI&&w->destArea==3-areaIndex&&w->destNode==w->id;
}
static int rocket_pipe_rim(const RocketSnapshot *car,struct Object *pipe) {
    unsigned visited=0;
    for(int z=0;z<NUM_CELLS;z++)for(int x=0;x<NUM_CELLS;x++)
        for(struct SurfaceNode *n=gDynamicSurfacePartition[z][x][SPATIAL_PARTITION_FLOORS].next;n;n=n->next) {
            if(++visited>(unsigned)(gSurfaceNodesAllocated>0?gSurfaceNodesAllocated:1))return 0;
            struct Surface *s=n->surface;
            if(!solid_surface(s)||s->object!=pipe||s->type!=SURFACE_WALL_MISC||
               !(s->flags&SURFACE_FLAG_DYNAMIC)||!isfinite(s->normal.y)||s->normal.y<.99f)continue;
            float top=s->vertex1[1];
            if(s->vertex2[1]!=top||s->vertex3[1]!=top||top<=pipe->oPosY+pipe->hitboxHeight||
               top>pipe->oPosY+512.f||car->position[1]<top-8.f||car->position[1]>top+70.f)continue;
            const s16 *v[]={s->vertex1,s->vertex2,s->vertex3};float triangle[3][3];
            for(int i=0;i<3;i++)for(int k=0;k<3;k++)triangle[i][k]=v[i][k];
            if(rocket_car_triangle_overlap(car,triangle,3.f))return 1;
            const float down[3]={0,-1,0};
            for(int i=0;i<4;i++)if(car->wheel_contacts[i]&&isfinite(car->wheel_radius[i])&&
                car->wheel_radius[i]>0&&car->wheel_radius[i]<=64.f) {
                float distance=door_ray_distance(s,car->wheel_position[i],down,car->wheel_radius[i]+3.f);
                if(distance>=car->wheel_radius[i]-3.f&&distance<=car->wheel_radius[i]+3.f)return 1;
            }
        }
    return 0;
}
static int rocket_pipe_interaction(struct MarioState *m) {
    if(m->playerIndex||m->area!=gCurrentArea||
       (gCurrLevelNum!=LEVEL_BITDW&&gCurrLevelNum!=LEVEL_BITS&&gCurrLevelNum!=LEVEL_THI)||
       sCurrPlayMode!=PLAY_MODE_NORMAL||m->hurtCounter||m->healCounter||m->squishTimer||
       m->quicksandDepth>0||(m->input&INPUT_SQUISHED)||m->skipWarpInteractionsTimer||
       gWarpTransition.isActive||sWarpDest.type!=WARP_TYPE_NOT_WARPING||sDelayedWarpOp!=WARP_OP_NONE||
       (gTimeStopState&TIME_STOP_ACTIVE))return 0;
    if(gCLIOpts.characterNet&&!gCLIOpts.offline&&(gNetworkType==NT_NONE||!gNetworkAreaLoaded||
       gNetworkAreaSyncing||!gNetworkPlayerLocal||!gNetworkPlayerLocal->connected||
       !gNetworkPlayerLocal->currLevelSyncValid||!gNetworkPlayerLocal->currAreaSyncValid||
       gNetworkPlayerLocal->currLevelNum!=gCurrLevelNum||gNetworkPlayerLocal->currAreaIndex!=m->area->index))return 0;
    RocketSnapshot car;
    if(!rocket_adapter_interaction_snapshot(&car)||!rocket_body_pose_valid(&car)||car.basis[7]<.95f||
       car.flipping||!isfinite(car.velocity[1])||car.velocity[1]>30.f)return 0;
    float moved=0;for(int k=0;k<3;k++){float d=m->pos[k]-car.position[k];moved+=d*d;}
    if(!isfinite(moved)||moved>4.f||m->marioObj->hitboxRadius<=0||m->marioObj->hitboxRadius>64.f)return 0;
    for(struct ObjectWarpNode *node=m->area->warpNodes;node;node=node->next) {
        if(!rocket_pipe_route(node,m->area->index))continue;
        struct Object *pipe=node->object;
        if(!pipe||pipe->behavior!=bhvWarpPipe||pipe->collisionData!=warp_pipe_seg3_collision_03009AC8||
           !(pipe->activeFlags&ACTIVE_FLAG_ACTIVE)||(pipe->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))||
           pipe->header.gfx.activeAreaIndex!=m->area->index||pipe->oIntangibleTimer||pipe->oSyncDeath||
           (pipe->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE)||pipe->oInteractType!=INTERACT_WARP||
           pipe->oInteractionSubtype||(u32)pipe->oBehParams!=((u32)node->node.id<<16)||
           (pipe->oInteractStatus&INT_STATUS_INTERACTED)||pipe->hitboxRadius!=50||pipe->hitboxHeight!=50||
           pipe->hitboxDownOffset!=0||(s16)pipe->oFaceAnglePitch||(s16)pipe->oFaceAngleRoll)continue;
        if(pipe->header.gfx.scale[0]!=1||pipe->header.gfx.scale[1]!=1||pipe->header.gfx.scale[2]!=1)continue;
        float dx=car.position[0]-pipe->oPosX,dz=car.position[2]-pipe->oPosZ;
        float radius=pipe->hitboxRadius+m->marioObj->hitboxRadius;
        /* Keep native horizontal reach. Only proven contact on the loaded rim
         * bridges the height gap; passing above/beside a pipe is insufficient. */
        if(!isfinite(dx)||!isfinite(dz)||dx*dx+dz*dz>=radius*radius||
           !rocket_pipe_rim(&car,pipe)||!rocket_adapter_enemy_visible(car.position,pipe,0))continue;
        int i=0;for(;i<m->marioObj->numCollidedObjs;i++)if(m->marioObj->collidedObjs[i]==pipe)break;
        if(i==m->marioObj->numCollidedObjs){if(i>=4)return 0;m->marioObj->collidedObjs[i]=pipe;m->marioObj->numCollidedObjs++;}
        m->marioObj->collidedObjInteractTypes|=INTERACT_WARP;m->collidedObjInteractTypes|=INTERACT_WARP;
        return 1;
    }
    return 0;
}
