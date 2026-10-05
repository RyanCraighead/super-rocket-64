/* Authored CCM entrance bridge, not a generic enlarged warp hitbox. The upright
 * Octane box is 241x173 host units; this native shaft is only 205x205. Preserve
 * collision geometry and let the existing local native warp handler do entry. */
static int ccm_chimney_face(const struct Surface *s,int rim) {
    if(!solid_surface(s)||s->object||(s->flags&SURFACE_FLAG_DYNAMIC)||!isfinite(s->normal.y)||s->normal.y<.99f||
       s->type!=(rim?SURFACE_HARD:SURFACE_WALL_MISC))return 0;
    /* The four shaft corners followed by the four outside rim corners. Only
     * actually loaded, authored triangles count as contact or shaft floor. */
    static const s16 corners[8][2]={
        {-283,-1588},{-78,-1588},{-283,-1383},{-78,-1383},
        {-385,-1690},{23,-1690},{-385,-1280},{23,-1280}
    };
    const s16 *vertices[]={s->vertex1,s->vertex2,s->vertex3};
    unsigned mask=0;
    for(int v=0;v<3;v++) {
        if(vertices[v][1]!=(rim?3123:2918))return 0;
        int i=0;for(;i<(rim?8:4);i++)
            if(vertices[v][0]==corners[i][0]&&vertices[v][2]==corners[i][1])break;
        if(i==(rim?8:4)||(mask&(1u<<i)))return 0;
        mask|=1u<<i;
    }
    if(!rim)return mask==0x0e||mask==0x07;
    static const unsigned rimFaces[]={0x51,0x45,0x23,0x31,0xc4,0x8c,0x8a,0xa2};
    for(unsigned i=0;i<sizeof rimFaces/sizeof *rimFaces;i++)if(mask==rimFaces[i])return 1;
    return 0;
}
static int ccm_chimney_rim_contact(const RocketSnapshot *pose) {
    /* The authored rim crosses x=0. Use the correct [z][x] partition order and
     * visit all of its cells, not a single floor sample below the car center. */
    int x0=(-385+LEVEL_BOUNDARY_MAX)/CELL_SIZE,x1=(23+LEVEL_BOUNDARY_MAX)/CELL_SIZE;
    int z0=(-1690+LEVEL_BOUNDARY_MAX)/CELL_SIZE,z1=(-1280+LEVEL_BOUNDARY_MAX)/CELL_SIZE;
    unsigned visited=0;
    for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++)
        for(struct SurfaceNode *n=gStaticSurfacePartition[z][x][SPATIAL_PARTITION_FLOORS].next;n;n=n->next) {
            if(++visited>(unsigned)(gSurfaceNodesAllocated>0?gSurfaceNodesAllocated:1))return 0;
            struct Surface *s=n->surface;
            if(!ccm_chimney_face(s,1))continue;
            const s16 *v[]={s->vertex1,s->vertex2,s->vertex3};float triangle[3][3];
            for(int i=0;i<3;i++)for(int k=0;k<3;k++)triangle[i][k]=v[i][k];
            /* A car may balance on its chassis while all suspension rays miss
             * the rim. RocketSim's >=3-wheel grounded flag is not required. */
            if(rocket_car_triangle_overlap(pose,triangle,3.f))return 1;
            const float down[3]={0,-1,0};
            for(int i=0;i<4;i++)if(pose->wheel_contacts[i]&&isfinite(pose->wheel_radius[i])&&
                pose->wheel_radius[i]>0&&pose->wheel_radius[i]<=64.f) {
                float distance=door_ray_distance(s,pose->wheel_position[i],down,pose->wheel_radius[i]+3.f);
                if(distance>=pose->wheel_radius[i]-3.f&&distance<=pose->wheel_radius[i]+3.f)return 1;
            }
        }
    return 0;
}
static int ccm_chimney_interaction(struct MarioState *m) {
    if(m->playerIndex!=0||gCurrLevelNum!=LEVEL_CCM||m->area!=gCurrentArea||m->area->index!=1||
       sCurrPlayMode!=PLAY_MODE_NORMAL||m->hurtCounter||m->healCounter||m->squishTimer||
       m->quicksandDepth>0||(m->input&INPUT_SQUISHED)||m->skipWarpInteractionsTimer||
       gWarpTransition.isActive||sWarpDest.type!=WARP_TYPE_NOT_WARPING||sDelayedWarpOp!=WARP_OP_NONE||
       (gTimeStopState&TIME_STOP_ACTIVE))return 0;
    if(gCLIOpts.characterNet&&!gCLIOpts.offline&&(gNetworkType==NT_NONE||!gNetworkAreaLoaded||gNetworkAreaSyncing||!gNetworkPlayerLocal||
       !gNetworkPlayerLocal->connected||!gNetworkPlayerLocal->currLevelSyncValid||
       !gNetworkPlayerLocal->currAreaSyncValid||gNetworkPlayerLocal->currLevelNum!=LEVEL_CCM||
       gNetworkPlayerLocal->currAreaIndex!=1))return 0;
    struct ObjectWarpNode *node=area_get_warp_node(0x1e);
    if(!node||node->node.id!=0x1e||(node->node.destLevel&0x7f)!=LEVEL_CCM||
       node->node.destArea!=2||node->node.destNode!=0x0a)return 0;
    struct Object *warp=node->object;
    if(!warp||warp->behavior!=bhvWarp||warp->oBehParams!=0x0f1e0000||
       !(warp->activeFlags&ACTIVE_FLAG_ACTIVE)||warp->header.gfx.activeAreaIndex!=1||
       warp->oInteractType!=INTERACT_WARP||warp->oIntangibleTimer||
       (warp->oInteractStatus&INT_STATUS_INTERACTED)||warp->oPosX!=-181||warp->oPosY!=2918||
       warp->oPosZ!=-1486||warp->hitboxRadius!=150||warp->hitboxHeight!=50||warp->hitboxDownOffset!=0)return 0;
    RocketSnapshot pose;
    if(!rocket_adapter_interaction_snapshot(&pose)||!rocket_body_pose_valid(&pose)||
       pose.basis[7]<.95f||pose.flipping||!isfinite(pose.velocity[1])||pose.velocity[1]>30.f||
       pose.position[1]<3115.f||pose.position[1]>3193.f)return 0;
    /* Use the actual opening, inset one unit for native integer floor queries.
     * A capsule-width margin rejected valid car rests near its edges, making
     * a slightly different landing after re-entry appear to disable the warp.
     * The car is handed to the native warp; no capsule is lowered into walls.
     * Loaded shaft floor, real rim contact and clear ray are still mandatory. */
    if(pose.position[0]<=-282.f||pose.position[0]>=-79.f||
       pose.position[2]<=-1587.f||pose.position[2]>=-1384.f)return 0;
    float moved=0;for(int k=0;k<3;k++){float d=m->pos[k]-pose.position[k];moved+=d*d;}
    if(!isfinite(moved)||moved>4.f)return 0;
    struct Surface *floor=NULL;
    float height=find_floor(pose.position[0],pose.position[1]+80.f,pose.position[2],&floor);
    if(height!=2918.f||!ccm_chimney_face(floor,0)||!ccm_chimney_rim_contact(&pose))return 0;
    /* The full 3D ray rejects static/dynamic obstructions down the shaft; no
     * Vanish permission may bypass them. The floor itself is below the target. */
    if(!rocket_adapter_enemy_visible(pose.position,warp,0))return 0;
    int i=0;for(;i<m->marioObj->numCollidedObjs;i++)if(m->marioObj->collidedObjs[i]==warp)break;
    if(i==m->marioObj->numCollidedObjs) {
        if(i>=4)return 0;
        m->marioObj->collidedObjs[i]=warp;m->marioObj->numCollidedObjs++;
    }
    m->marioObj->collidedObjInteractTypes|=INTERACT_WARP;
    m->collidedObjInteractTypes|=INTERACT_WARP;
    return 1; // Native interact_warp owns action, timer, hooks and destination.
}
