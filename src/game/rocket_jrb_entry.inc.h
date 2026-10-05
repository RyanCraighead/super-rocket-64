/* JRB's tilted 205-unit porthole can stop the Octane chassis before Mario's
 * center reaches the floor warp at the back. Recognize the actual loaded
 * entrance and use its native warp operation; never resize collision or move
 * the player, eel, chests, stars, or network peers. */
static void jrb_ship_point(const struct Object *ship,const float local[3],float world[3]) {
    for(int k=0;k<3;k++) {
        world[k]=ship->transform[3][k];
        for(int a=0;a<3;a++)world[k]+=local[a]*ship->transform[a][k];
    }
}
static int jrb_ship_warp_face(const struct Surface *s,const struct Object *ship) {
    if(!solid_surface(s)||s->object!=ship||s->type!=SURFACE_WARP||
       !(s->flags&SURFACE_FLAG_DYNAMIC)||!isfinite(s->normal.y)||s->normal.y<.5f)return 0;
    const s16 *vertices[]={s->vertex1,s->vertex2,s->vertex3};
    unsigned mask=0;
    for(int v=0;v<3;v++) {
        float p[3]={0};
        for(int a=0;a<3;a++)for(int k=0;k<3;k++)
            p[a]+=(vertices[v][k]-ship->transform[3][k])*ship->transform[a][k];
        if(fabsf(p[1]-819.f)>2.f)return 0;
        int x=fabsf(p[0]-307.f)<=2.f?0:fabsf(p[0]-512.f)<=2.f?1:-1;
        int z=fabsf(p[2]+409.f)<=2.f?0:fabsf(p[2]+255.f)<=2.f?1:-1;
        if(x<0||z<0)return 0;
        mask|=1u<<(x+2*z);
    }
    return mask==7?1:mask==14?2:0;
}
static int jrb_ship_path_clear(const float from[3],const float to[3]) {
    float direction[3],length=0;
    for(int k=0;k<3;k++){direction[k]=to[k]-from[k];length+=direction[k]*direction[k];}
    length=sqrtf(length);if(!isfinite(length)||length<1.f)return 0;
    for(int k=0;k<3;k++)direction[k]/=length;
    unsigned visited=0;
    for(int dynamic=0;dynamic<2;dynamic++) {
        SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
        for(int z=0;z<NUM_CELLS;z++)for(int x=0;x<NUM_CELLS;x++)for(int p=0;p<3;p++)
            for(struct SurfaceNode *n=partition[z][x][p].next;n;n=n->next) {
                if(++visited>(unsigned)gSurfaceNodesAllocated)return 0;
                if(solid_surface(n->surface)&&door_ray_distance(n->surface,from,direction,length)<length)return 0;
            }
    }
    return 1;
}
static int jrb_ship_interaction(struct MarioState *m) {
    if(m->playerIndex!=0||gCurrLevelNum!=LEVEL_JRB||gCurrActNum!=1||m->area!=gCurrentArea||m->area->index!=1||
       sCurrPlayMode!=PLAY_MODE_NORMAL||m->hurtCounter||m->healCounter||m->squishTimer||
       m->quicksandDepth>0||(m->input&INPUT_SQUISHED)||m->skipWarpInteractionsTimer||
       gWarpTransition.isActive||sWarpDest.type!=WARP_TYPE_NOT_WARPING||sDelayedWarpOp!=WARP_OP_NONE||
       (gTimeStopState&TIME_STOP_ACTIVE)||gSurfaceNodesAllocated<=0)return 0;
    if(gCLIOpts.characterNet&&!gCLIOpts.offline&&(gNetworkType==NT_NONE||!gNetworkAreaLoaded||gNetworkAreaSyncing||
       !gNetworkPlayerLocal||!gNetworkPlayerLocal->connected||!gNetworkPlayerLocal->currLevelSyncValid||
       !gNetworkPlayerLocal->currAreaSyncValid||gNetworkPlayerLocal->currLevelNum!=LEVEL_JRB||
       gNetworkPlayerLocal->currAreaIndex!=1))return 0;
    struct ObjectWarpNode *warp=area_get_warp_node(WARP_NODE_WARP_FLOOR);
    if(!warp||warp->node.id!=WARP_NODE_WARP_FLOOR||(warp->node.destLevel&0x7f)!=LEVEL_JRB||
       warp->node.destArea!=2||warp->node.destNode!=0x0a)return 0;
    RocketSnapshot pose;
    if(!rocket_adapter_interaction_snapshot(&pose)||!rocket_body_pose_valid(&pose)||pose.flipping||
       pose.position[1]>=m->waterLevel-100.f)return 0;
    float center[3],moved=0;
    for(int k=0;k<3;k++) {
        float d=m->pos[k]-pose.position[k];moved+=d*d;
        center[k]=pose.position[k]+pose.basis[k]*ROCKET_BODY_FORWARD_OFFSET+pose.basis[6+k]*ROCKET_BODY_UP_OFFSET;
    }
    if(!isfinite(moved)||moved>4.f)return 0;
    struct Object *ship=NULL;int eelDeparted=0;
    unsigned visited=0;
    for(int list=0;list<NUM_OBJ_LISTS;list++) {
        struct ObjectNode *head=&gObjectLists[list];
        for(struct ObjectNode *n=head->next;n&&n!=head;n=n->next) {
            if(++visited>OBJECT_POOL_CAPACITY)return 0;
            struct Object *o=(struct Object*)n;
            if(!(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->header.gfx.activeAreaIndex!=1)continue;
            if(o->behavior==bhvInSunkenShip)ship=o;
            if(o->behavior==bhvUnagi&&o->oBehParams2ndByte==0) {
                if(o->oAction!=1)return 0;
                eelDeparted=1;
            }
            /* The eel must also have physically cleared the mouth. */
            if(o->behavior==bhvUnagiSubobject) {
                float dx=o->oPosX-center[0],dy=o->oPosY-center[1],dz=o->oPosZ-center[2];
                if(!isfinite(dx+dy+dz)||dx*dx+dy*dy+dz*dz<400.f*400.f)return 0;
            }
        }
    }
    if(!ship||!eelDeparted||ship->oPosX!=5385||ship->oPosY!=-5520||ship->oPosZ!=2428||
       (s16)ship->oFaceAnglePitch!=(s16)0xe958||(s16)ship->oFaceAngleYaw!=(s16)0xee6c||ship->oFaceAngleRoll!=0x0c80)return 0;
    const float shipPosition[3]={ship->oPosX,ship->oPosY,ship->oPosZ};
    float local[3]={0};
    for(int a=0;a<3;a++) {
        if(ship->header.gfx.scale[a]!=1.f||!isfinite(ship->transform[3][a])||
           fabsf(ship->transform[3][a]-shipPosition[a])>.01f)return 0;
        for(int b=a;b<3;b++) {
            float dot=0;
            for(int k=0;k<3;k++)dot+=ship->transform[a][k]*ship->transform[b][k];
            if(!isfinite(dot)||fabsf(dot-(a==b?1.f:0.f))>.01f)return 0;
        }
        for(int k=0;k<3;k++)local[a]+=(center[k]-ship->transform[3][k])*ship->transform[a][k];
    }
    /* Centered in the opening, touching its front plane, on the outside of
     * the native warp floor. A nearby car above/beside/behind the hull fails. */
    if(local[0]<=327||local[0]>=492||local[1]<=839||local[1]>=1004||local[2]<-240||local[2]>80)return 0;
    static const float mouth[4][3]={{307,819,-101},{512,819,-101},{307,1024,-101},{512,1024,-101}};
    float triangle[3][3];int contact=0;
    for(int face=0;face<2;face++) {
        for(int v=0;v<3;v++)jrb_ship_point(ship,mouth[face+v],triangle[v]);
        contact|=rocket_car_triangle_overlap(&pose,triangle,3.f);
    }
    if(!contact)return 0;
    unsigned faces=0;visited=0;
    for(int z=0;z<NUM_CELLS;z++)for(int x=0;x<NUM_CELLS;x++)
        for(struct SurfaceNode *n=gDynamicSurfacePartition[z][x][SPATIAL_PARTITION_FLOORS].next;n;n=n->next) {
            if(++visited>(unsigned)gSurfaceNodesAllocated)return 0;
            faces|=jrb_ship_warp_face(n->surface,ship);
        }
    if(faces!=3)return 0;
    const float inside[3]={409.5f,921.5f,-332.f};float target[3];jrb_ship_point(ship,inside,target);
    if(!jrb_ship_path_clear(center,target))return 0;
    level_trigger_warp(m,WARP_OP_WARP_FLOOR);
    return 1;
}
