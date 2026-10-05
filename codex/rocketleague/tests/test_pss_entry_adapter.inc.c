/* Real adapter, native/runtime mocks from test_adapter.c. Only native warp
 * staging is asserted; no star flags, door unlocks or slide timers are written. */
static struct Object slideWarp,slideOther;
static struct ObjectWarpNode slideNode;
static struct Surface slideFaces[3];
static struct SurfaceNode slideFaceNodes[3];
static unsigned slideChecks;
#define SLIDE_CHECK(c) do { ++slideChecks; assert(c); } while(0)
static void slide_position(float side,float y,float depth) {
    float center[3]={1911.f+(depth+side)*.70710678118f,y,-2127.5f+(depth-side)*.70710678118f};
    for(int k=0;k<3;k++)pose.position[k]=mario.pos[k]=center[k]-
        pose.basis[k]*ROCKET_BODY_FORWARD_OFFSET-pose.basis[6+k]*ROCKET_BODY_UP_OFFSET;
}
static void slide_add_face(int i,const s16 a[3],const s16 b[3],const s16 c[3],int dynamic) {
    struct Surface *s=&slideFaces[i];memset(s,0,sizeof *s);
    memcpy(s->vertex1,a,sizeof(Vec3s));memcpy(s->vertex2,b,sizeof(Vec3s));memcpy(s->vertex3,c,sizeof(Vec3s));
    s->type=SURFACE_DEFAULT;s->normal.y=1;s->flags=dynamic?SURFACE_FLAG_DYNAMIC:0;
    slideFaceNodes[i].surface=s;
    SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
    slideFaceNodes[i].next=partition[0][0][SPATIAL_PARTITION_FLOORS].next;
    partition[0][0][SPATIAL_PARTITION_FLOORS].next=&slideFaceNodes[i];
}
static void slide_setup(enum NetworkType role) {
    fresh();gCurrLevelNum=LEVEL_CASTLE;testArea.index=1;step();mario.action=ACT_FREEFALL;pose.grounded=0;
    memset(&slideWarp,0,sizeof slideWarp);memset(&slideOther,0,sizeof slideOther);memset(&slideNode,0,sizeof slideNode);
    memset(slideFaceNodes,0,sizeof slideFaceNodes);memset(gStaticSurfacePartition,0,sizeof gStaticSurfacePartition);
    memset(gDynamicSurfacePartition,0,sizeof gDynamicSurfacePartition);memset(pose.basis,0,sizeof pose.basis);
    pose.basis[0]=pose.basis[2]=pose.basis[3]=.70710678118f;pose.basis[5]=-.70710678118f;pose.basis[7]=1;
    slide_position(0,845,-50);
    const s16 a[3]={1857,768,-2073},b[3]={2002,768,-1928},c[3]={2110,768,-2037},d[3]={1965,768,-2182};
    slide_add_face(0,a,b,c,0);slide_add_face(1,a,c,d,0);
    slideWarp.behavior=bhvWarp;slideWarp.activeFlags=ACTIVE_FLAG_ACTIVE;slideWarp.header.gfx.activeAreaIndex=1;
    slideWarp.oBehParams=0x000a0000;slideWarp.oInteractType=INTERACT_WARP;
    slideWarp.oPosX=2013;slideWarp.oPosY=768;slideWarp.oPosZ=-2014;slideWarp.hitboxRadius=50;slideWarp.hitboxHeight=50;
    slideNode.node.id=0xa;slideNode.node.destLevel=LEVEL_PSS;slideNode.node.destArea=1;slideNode.node.destNode=0xa;
    slideNode.object=&slideWarp;testArea.warpNodes=&slideNode;
    gNetworkType=role;gCLIOpts.characterNet=role!=NT_NONE;gCLIOpts.offline=role==NT_NONE;
    gNetworkAreaLoaded=true;gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerLocal->connected=true;
    gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=true;
    gNetworkPlayerLocal->currLevelNum=LEVEL_CASTLE;gNetworkPlayerLocal->currAreaIndex=1;
}
static void slide_try(int accepted) {
    struct MarioState before=mario;struct Object oldWarp=slideWarp;
    int beforeSteps=steps,beforeResets=resets,count=object.numCollidedObjs,beforeWarpOp=sDelayedWarpOp;
    rocket_adapter_prepare_interactions(&mario);
    SLIDE_CHECK(steps==beforeSteps&&resets==beforeResets&&!shipWarps);
    SLIDE_CHECK(!memcmp(before.pos,mario.pos,sizeof mario.pos)&&before.action==mario.action&&before.actionArg==mario.actionArg);
    SLIDE_CHECK(before.numStars==mario.numStars&&before.usedObj==mario.usedObj&&before.interactObj==mario.interactObj);
    SLIDE_CHECK(!memcmp(&oldWarp,&slideWarp,sizeof slideWarp)&&sDelayedWarpOp==beforeWarpOp);
    if(accepted) {
        SLIDE_CHECK(object.numCollidedObjs==count+1&&object.collidedObjs[count]==&slideWarp);
        SLIDE_CHECK((object.collidedObjInteractTypes&INTERACT_WARP)&&(mario.collidedObjInteractTypes&INTERACT_WARP));
    } else {
        SLIDE_CHECK(object.numCollidedObjs==count&&before.collidedObjInteractTypes==mario.collidedObjInteractTypes);
    }
}
static void slide_blocker(int dynamic,int vanish) {
    const s16 a[3]={1750,700,-1930},b[3]={2060,700,-2240},c[3]={1920,1100,-2100};
    slide_add_face(2,a,b,c,dynamic);slideFaces[2].type=vanish?SURFACE_VANISH_CAP_WALLS:SURFACE_DEFAULT;
}
static void test_pss_entry_adapter(void) {
    /* Sweeping the actual Octane box reaches the narrow side before its native
     * center can overlap the 50-unit warp cylinder. The mouth bridge works at
     * that first physical contact, without deleting or moving the wall. */
    slide_setup(NT_NONE);
    const s16 a[3]={1857,768,-2073},b[3]={2002,768,-1928},c[3]={1857,922,-2073};
    float triangle[3][3];const s16 *v[]={a,b,c};
    for(int i=0;i<3;i++)for(int k=0;k<3;k++)triangle[i][k]=v[i][k];
    float depth=-150;
    for(;depth<0;depth+=.25f){slide_position(0,845,depth);if(rocket_car_triangle_overlap(&pose,triangle,0))break;}
    SLIDE_CHECK(depth<0);
    SLIDE_CHECK(hypotf(mario.pos[0]-slideWarp.oPosX,mario.pos[2]-slideWarp.oPosZ)>100);
    slide_add_face(2,a,b,c,0);slide_try(1);
    for(int role=NT_NONE;role<=NT_CLIENT;role++) {
        slide_setup(role);slide_try(1);rocket_adapter_prepare_interactions(&mario);SLIDE_CHECK(object.numCollidedObjs==1);
    }
    slide_setup(NT_SERVER);gCLIOpts.offline=true;gNetworkPlayerLocal=NULL;gNetworkAreaLoaded=false;slide_try(1);
#define SLIDE_REJECT(change) do { slide_setup(NT_CLIENT);change;slide_try(0); } while(0)
    SLIDE_REJECT(gCurrLevelNum=LEVEL_JRB);SLIDE_REJECT(testArea.index=2);SLIDE_REJECT(gCurrentArea=NULL);
    SLIDE_REJECT(mario.playerIndex=1);SLIDE_REJECT(mario.health=0xff);SLIDE_REJECT(mario.freeze=1);
    SLIDE_REJECT(mario.action=ACT_READING_AUTOMATIC_DIALOG);SLIDE_REJECT(mario.action=ACT_DISAPPEARED);
    SLIDE_REJECT(mario.action=ACT_BACKWARD_GROUND_KB);SLIDE_REJECT(mario.heldObj=&slideOther);
    SLIDE_REJECT(mario.heldByObj=&slideOther);SLIDE_REJECT(mario.riddenObj=&slideOther);
    SLIDE_REJECT(mario.hurtCounter=1);SLIDE_REJECT(mario.healCounter=1);SLIDE_REJECT(mario.squishTimer=1);
    SLIDE_REJECT(mario.quicksandDepth=1);SLIDE_REJECT(mario.input|=INPUT_SQUISHED);SLIDE_REJECT(mario.skipWarpInteractionsTimer=1);
    SLIDE_REJECT(uiBlocked=1);SLIDE_REJECT(enabled=0);SLIDE_REJECT(rocket_adapter_set_selected(0));
    SLIDE_REJECT(rocket_adapter_suspend());SLIDE_REJECT(gGlobalTimer+=2);SLIDE_REJECT(sCurrPlayMode=PLAY_MODE_PAUSED);
    SLIDE_REJECT(gWarpTransition.isActive=1);SLIDE_REJECT(sWarpDest.type=1);
    SLIDE_REJECT(gTimeStopState=TIME_STOP_ACTIVE);SLIDE_REJECT(sDelayedWarpOp=WARP_OP_WARP_OBJECT);
    SLIDE_REJECT(gNetworkType=NT_NONE);SLIDE_REJECT(gNetworkAreaLoaded=false);SLIDE_REJECT(gNetworkAreaSyncing=true);
    SLIDE_REJECT(gNetworkPlayerLocal=NULL);SLIDE_REJECT(gNetworkPlayerLocal->connected=false);
    SLIDE_REJECT(gNetworkPlayerLocal->currLevelSyncValid=false);SLIDE_REJECT(gNetworkPlayerLocal->currAreaSyncValid=false);
    SLIDE_REJECT(gNetworkPlayerLocal->currLevelNum=LEVEL_PSS);SLIDE_REJECT(gNetworkPlayerLocal->currAreaIndex=2);
    SLIDE_REJECT(slideNode.node.id=0xb);SLIDE_REJECT(slideNode.node.destLevel=LEVEL_JRB);
    SLIDE_REJECT(slideNode.node.destArea=2);SLIDE_REJECT(slideNode.node.destNode=0xb);SLIDE_REJECT(slideNode.object=NULL);
    SLIDE_REJECT(slideWarp.behavior=bhvWingCap);SLIDE_REJECT(slideWarp.activeFlags=0);SLIDE_REJECT(slideWarp.header.gfx.activeAreaIndex=2);
    SLIDE_REJECT(slideWarp.oInteractType=INTERACT_WARP_DOOR);SLIDE_REJECT(slideWarp.oBehParams^=0x10000);
    SLIDE_REJECT(slideWarp.oIntangibleTimer=-1);SLIDE_REJECT(slideWarp.oInteractStatus=INT_STATUS_INTERACTED);
    SLIDE_REJECT(slideWarp.oInteractionSubtype=INT_SUBTYPE_FADING_WARP);
    SLIDE_REJECT(slideWarp.oPosX++);SLIDE_REJECT(slideWarp.oPosY++);SLIDE_REJECT(slideWarp.oPosZ++);
    SLIDE_REJECT(slideWarp.hitboxRadius=150);SLIDE_REJECT(slideWarp.hitboxHeight=100);SLIDE_REJECT(slideWarp.hitboxDownOffset=20);
    SLIDE_REJECT(slide_position(100,845,-50));SLIDE_REJECT(slide_position(-100,845,-50));
    SLIDE_REJECT(slide_position(0,700,-50));SLIDE_REJECT(slide_position(0,1100,-50));
    SLIDE_REJECT(slide_position(0,845,-200));SLIDE_REJECT(slide_position(0,845,300));
    SLIDE_REJECT(slide_position(0,845,-140)); // Nearby in the air; chassis has not reached the mouth.
    SLIDE_REJECT(mario.pos[0]+=3);SLIDE_REJECT(pose.position[0]=NAN);SLIDE_REJECT(pose.basis[0]=NAN);
    SLIDE_REJECT(pose.flipping=1);SLIDE_REJECT(slideFaces[0].type=SURFACE_HARD);SLIDE_REJECT(slideFaces[0].object=&slideOther);
    SLIDE_REJECT(slideFaces[0].normal.y=NAN);SLIDE_REJECT(slideFaces[1].flags=SURFACE_FLAG_DYNAMIC);
    SLIDE_REJECT(slideFaces[0].vertex1[0]++);SLIDE_REJECT(gSurfaceNodesAllocated=1);
    SLIDE_REJECT(slideFaceNodes[0].next=&slideFaceNodes[0]);
    SLIDE_REJECT(slide_blocker(0,0));SLIDE_REJECT(slide_blocker(1,0));SLIDE_REJECT(slide_blocker(1,1);mario.flags=MARIO_VANISH_CAP);
    SLIDE_REJECT(object.numCollidedObjs=4;for(int i=0;i<4;i++)object.collidedObjs[i]=&slideOther);
    /* The ordinary one-star room door is outside this entrance's bounded reach. */
    SLIDE_REJECT(mario.numStars=0;slide_position(0,695,-600));
    slide_setup(NT_SERVER);mario.numStars=120;slide_try(1); // Never changes the star count.
    printf("PASS PSS stained-glass adapter: %u checks; real chassis contact, exact alcove, native staging, blockers and local offline/host/client\n",slideChecks);
    fresh();
#undef SLIDE_REJECT
}
#undef SLIDE_CHECK
