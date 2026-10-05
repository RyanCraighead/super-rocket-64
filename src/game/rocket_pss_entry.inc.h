/* Princess's Secret Slide is an object warp in the castle's right stained-glass
 * alcove. Its diagonal mouth is about 154 units wide (Octane is 173). Keep the
 * castle collision and one-star room door unchanged; stage only this native
 * warp when the chassis reaches the loaded alcove from inside the room. */
static int pss_alcove_floor_face(const struct Surface *s) {
    if(!solid_surface(s)||s->object||(s->flags&SURFACE_FLAG_DYNAMIC)||s->type!=SURFACE_DEFAULT||
       !isfinite(s->normal.y)||s->normal.y<.99f)return 0;
    static const s16 corners[4][2]={{1857,-2073},{2002,-1928},{2110,-2037},{1965,-2182}};
    const s16 *vertices[]={s->vertex1,s->vertex2,s->vertex3};unsigned mask=0;
    for(int v=0;v<3;v++) {
        if(vertices[v][1]!=768)return 0;
        int i=0;for(;i<4;i++)if(vertices[v][0]==corners[i][0]&&vertices[v][2]==corners[i][1])break;
        if(i==4||(mask&(1u<<i)))return 0;
        mask|=1u<<i;
    }
    return mask==7?1:mask==13?2:0;
}
static int pss_alcove_interaction(struct MarioState *m) {
    if(m->playerIndex!=0||gCurrLevelNum!=LEVEL_CASTLE||m->area!=gCurrentArea||m->area->index!=1||
       sCurrPlayMode!=PLAY_MODE_NORMAL||m->hurtCounter||m->healCounter||m->squishTimer||
       m->quicksandDepth>0||(m->input&INPUT_SQUISHED)||m->skipWarpInteractionsTimer||
       gWarpTransition.isActive||sWarpDest.type!=WARP_TYPE_NOT_WARPING||sDelayedWarpOp!=WARP_OP_NONE||
       (gTimeStopState&TIME_STOP_ACTIVE)||gSurfaceNodesAllocated<=0)return 0;
    if(gCLIOpts.characterNet&&!gCLIOpts.offline&&(gNetworkType==NT_NONE||!gNetworkAreaLoaded||gNetworkAreaSyncing||
       !gNetworkPlayerLocal||!gNetworkPlayerLocal->connected||!gNetworkPlayerLocal->currLevelSyncValid||
       !gNetworkPlayerLocal->currAreaSyncValid||gNetworkPlayerLocal->currLevelNum!=LEVEL_CASTLE||
       gNetworkPlayerLocal->currAreaIndex!=1))return 0;
    struct ObjectWarpNode *node=area_get_warp_node(0x0a);
    if(!node||node->node.id!=0x0a||(node->node.destLevel&0x7f)!=LEVEL_PSS||
       node->node.destArea!=1||node->node.destNode!=0x0a)return 0;
    struct Object *warp=node->object;
    if(!warp||warp->behavior!=bhvWarp||warp->oBehParams!=0x000a0000||
       !(warp->activeFlags&ACTIVE_FLAG_ACTIVE)||warp->header.gfx.activeAreaIndex!=1||
       warp->oInteractType!=INTERACT_WARP||warp->oIntangibleTimer||warp->oInteractionSubtype||
       (warp->oInteractStatus&INT_STATUS_INTERACTED)||warp->oPosX!=2013||warp->oPosY!=768||
       warp->oPosZ!=-2014||warp->hitboxRadius!=50||warp->hitboxHeight!=50||warp->hitboxDownOffset!=0)return 0;
    RocketSnapshot pose;
    if(!rocket_adapter_interaction_snapshot(&pose)||!rocket_body_pose_valid(&pose)||pose.flipping)return 0;
    float center[3],moved=0;
    for(int k=0;k<3;k++) {
        float d=m->pos[k]-pose.position[k];moved+=d*d;
        center[k]=pose.position[k]+pose.basis[k]*ROCKET_BODY_FORWARD_OFFSET+pose.basis[6+k]*ROCKET_BODY_UP_OFFSET;
    }
    if(!isfinite(moved)||moved>4.f)return 0;
    float side=((center[0]-1911.f)-(center[2]+2127.5f))*.70710678118f;
    float depth=((center[0]-1911.f)+(center[2]+2127.5f))*.70710678118f;
    if(fabsf(side)>=56.f||depth<-150.f||depth>60.f||center[1]<788.f||center[1]>922.f)return 0;
    const float mouth[4][3]={{1857,768,-2073},{1965,768,-2182},{1857,922,-2073},{1965,922,-2182}};
    float triangle[3][3];int contact=0;
    for(int face=0;face<2;face++) {
        for(int v=0;v<3;v++)for(int k=0;k<3;k++)triangle[v][k]=mouth[face+v][k];
        contact|=rocket_car_triangle_overlap(&pose,triangle,3.f);
    }
    if(!contact)return 0;
    unsigned faces=0,visited=0;
    for(int z=0;z<NUM_CELLS;z++)for(int x=0;x<NUM_CELLS;x++)
        for(struct SurfaceNode *n=gStaticSurfacePartition[z][x][SPATIAL_PARTITION_FLOORS].next;n;n=n->next) {
            if(++visited>(unsigned)gSurfaceNodesAllocated)return 0;
            faces|=pss_alcove_floor_face(n->surface);
        }
    if(faces!=3||!rocket_adapter_enemy_visible(center,warp,0))return 0;
    int i=0;for(;i<m->marioObj->numCollidedObjs;i++)if(m->marioObj->collidedObjs[i]==warp)break;
    if(i==m->marioObj->numCollidedObjs) {
        if(i>=4)return 0;
        m->marioObj->collidedObjs[i]=warp;m->marioObj->numCollidedObjs++;
    }
    m->marioObj->collidedObjInteractTypes|=INTERACT_WARP;
    m->collidedObjInteractTypes|=INTERACT_WARP;
    return 1; // Native interact_warp owns destination, action, timers and hooks.
}
