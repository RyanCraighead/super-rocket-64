/* Actual adapter and authored CCM triangles, with explicit native/runtime mocks
 * in test_adapter.c. This checks interaction staging, not a running Bullet car. */
static struct Object chimneyWarp,chimneyOther;
static struct ObjectWarpNode chimneyNode;
static struct Surface chimneyRim[8],chimneyBottom,chimneyBlocker;
static struct SurfaceNode chimneyNodes[9];
static unsigned chimneyChecks;
#define CCM_CHECK(c) do { ++chimneyChecks; assert(c); } while(0)
static void chimney_position(float x,float y,float z){
    pose.position[0]=mario.pos[0]=x;pose.position[1]=mario.pos[1]=y;pose.position[2]=mario.pos[2]=z;
}
static void chimney_setup(enum NetworkType role){
    fresh();gCurrLevelNum=LEVEL_CCM;testArea.index=1;
    mario.pos[0]=-181;mario.pos[1]=3120.15f;mario.pos[2]=-1486;step();
    chimney_position(-181,3120.15f,-1486);mario.action=ACT_FREEFALL;
    pose.grounded=0;memset(pose.wheel_contacts,0,sizeof pose.wheel_contacts);
    memset(pose.velocity,0,sizeof pose.velocity);
    memset(&chimneyWarp,0,sizeof chimneyWarp);memset(&chimneyOther,0,sizeof chimneyOther);
    memset(&chimneyNode,0,sizeof chimneyNode);memset(chimneyRim,0,sizeof chimneyRim);
    memset(&chimneyBottom,0,sizeof chimneyBottom);memset(&chimneyBlocker,0,sizeof chimneyBlocker);
    memset(chimneyNodes,0,sizeof chimneyNodes);
    memset(gStaticSurfacePartition,0,sizeof gStaticSurfacePartition);
    memset(gDynamicSurfacePartition,0,sizeof gDynamicSurfacePartition);
    /* Coordinates of all eight original rim faces, independently listed from
     * the native collision stream. A fabricated rectangular annulus is not used. */
    static const s16 rim[8][3][3]={
        {{-283,3123,-1588},{-385,3123,-1690},{-385,3123,-1280}},
        {{-283,3123,-1588},{-385,3123,-1280},{-283,3123,-1383}},
        {{-283,3123,-1588},{-78,3123,-1588},{23,3123,-1690}},
        {{-385,3123,-1690},{-283,3123,-1588},{23,3123,-1690}},
        {{23,3123,-1280},{-283,3123,-1383},{-385,3123,-1280}},
        {{23,3123,-1280},{-78,3123,-1383},{-283,3123,-1383}},
        {{-78,3123,-1588},{-78,3123,-1383},{23,3123,-1280}},
        {{23,3123,-1690},{-78,3123,-1588},{23,3123,-1280}}
    };
    int z=(-1486+LEVEL_BOUNDARY_MAX)/CELL_SIZE;
    for(int i=0;i<8;i++){
        memcpy(chimneyRim[i].vertex1,rim[i][0],sizeof(Vec3s));
        memcpy(chimneyRim[i].vertex2,rim[i][1],sizeof(Vec3s));
        memcpy(chimneyRim[i].vertex3,rim[i][2],sizeof(Vec3s));
        chimneyRim[i].normal.y=1;chimneyRim[i].type=SURFACE_HARD;
        /* Right faces deliberately reside across x=0; bounded scans must use
         * [z][x] and include that cell even though the car center is x<0. */
        int x=((i>=6?23:-181)+LEVEL_BOUNDARY_MAX)/CELL_SIZE;
        chimneyNodes[i].surface=&chimneyRim[i];
        chimneyNodes[i].next=gStaticSurfacePartition[z][x][SPATIAL_PARTITION_FLOORS].next;
        gStaticSurfacePartition[z][x][SPATIAL_PARTITION_FLOORS].next=&chimneyNodes[i];
    }
    vec3s_set(chimneyBottom.vertex1,-78,2918,-1383);
    vec3s_set(chimneyBottom.vertex2,-78,2918,-1588);
    vec3s_set(chimneyBottom.vertex3,-283,2918,-1383);
    chimneyBottom.normal.y=1;chimneyBottom.type=SURFACE_WALL_MISC;
    nativeFloor=&chimneyBottom;chimneyGeometry=1;chimneyFloorHeight=2918;
    chimneyWarp.behavior=bhvWarp;chimneyWarp.activeFlags=ACTIVE_FLAG_ACTIVE;
    chimneyWarp.header.gfx.activeAreaIndex=1;chimneyWarp.oBehParams=0x0f1e0000;
    chimneyWarp.oInteractType=INTERACT_WARP;chimneyWarp.oPosX=-181;chimneyWarp.oPosY=2918;chimneyWarp.oPosZ=-1486;
    chimneyWarp.hitboxRadius=150;chimneyWarp.hitboxHeight=50;
    chimneyNode.node.id=0x1e;chimneyNode.node.destLevel=LEVEL_CCM;
    chimneyNode.node.destArea=2;chimneyNode.node.destNode=0x0a;chimneyNode.object=&chimneyWarp;
    testArea.warpNodes=&chimneyNode;
    gNetworkType=role;gCLIOpts.characterNet=role!=NT_NONE;gCLIOpts.offline=role==NT_NONE;
    gNetworkAreaLoaded=true;gNetworkPlayerLocal=&gNetworkPlayers[0];
    gNetworkPlayerLocal->connected=true;gNetworkPlayerLocal->currLevelSyncValid=true;
    gNetworkPlayerLocal->currAreaSyncValid=true;gNetworkPlayerLocal->currLevelNum=LEVEL_CCM;
    gNetworkPlayerLocal->currAreaIndex=1;gNetworkPlayerLocal->globalIndex=role==NT_CLIENT?1:0;
}
static void chimney_try(int accepted){
    struct MarioState before=mario;struct Object warpBefore=chimneyWarp;
    int oldSteps=steps,oldResets=resets,count=object.numCollidedObjs;
    rocket_adapter_prepare_interactions(&mario);
    CCM_CHECK(steps==oldSteps&&resets==oldResets);
    CCM_CHECK(!memcmp(before.pos,mario.pos,sizeof mario.pos)&&before.action==mario.action&&before.actionArg==mario.actionArg);
    CCM_CHECK(mario.usedObj==before.usedObj&&mario.interactObj==before.interactObj);
    CCM_CHECK(!memcmp(&warpBefore,&chimneyWarp,sizeof chimneyWarp));
    if(accepted){
        CCM_CHECK(object.numCollidedObjs==count+1&&object.collidedObjs[count]==&chimneyWarp);
        CCM_CHECK((object.collidedObjInteractTypes&INTERACT_WARP)&&(mario.collidedObjInteractTypes&INTERACT_WARP));
    } else {
        CCM_CHECK(object.numCollidedObjs==count);
        CCM_CHECK(object.collidedObjInteractTypes==0&&mario.collidedObjInteractTypes==before.collidedObjInteractTypes);
    }
}
static void chimney_block(int dynamic,int vanish){
    vec3s_set(chimneyBlocker.vertex1,-350,3050,-1750);
    vec3s_set(chimneyBlocker.vertex2,150,3050,-1750);
    vec3s_set(chimneyBlocker.vertex3,-100,3050,-1100);
    chimneyBlocker.type=vanish?SURFACE_VANISH_CAP_WALLS:SURFACE_DEFAULT;
    chimneyBlocker.normal.y=1;
    chimneyNodes[8].surface=&chimneyBlocker;
    SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
    int z=(-1486+LEVEL_BOUNDARY_MAX)/CELL_SIZE,x=(-181+LEVEL_BOUNDARY_MAX)/CELL_SIZE;
    chimneyNodes[8].next=partition[z][x][SPATIAL_PARTITION_FLOORS].next;
    partition[z][x][SPATIAL_PARTITION_FLOORS].next=&chimneyNodes[8];
}
static void chimney_single_case(int wheel){
    chimney_setup(NT_NONE);
    if(wheel){
        chimney_position(-181,3157,-1486);pose.grounded=1;
        pose.wheel_contacts[0]=1;pose.wheel_radius[0]=30;
        pose.wheel_position[0][0]=-181;pose.wheel_position[0][1]=3153;pose.wheel_position[0][2]=-1320;
    }
    chimney_try(1);
    printf("PASS CCM public interaction staging: %s\n",wheel?"upright wheel rest":"body rest, no grounded wheels");
}
static void test_ccm_chimney_adapter(void){
    for(int role=NT_NONE;role<=NT_CLIENT;role++){
        chimney_setup(role);CCM_CHECK(!pose.grounded&&!controller.rawStickY);
        chimney_try(1);rocket_adapter_prepare_interactions(&mario);CCM_CHECK(object.numCollidedObjs==1);
        /* Wheel rest is separately accepted with the chassis completely above
         * the rim. Three grounded tires are not necessary to touch a surface. */
        chimney_setup(role);chimney_position(-181,3157,-1486);pose.grounded=1;
        pose.wheel_contacts[0]=1;pose.wheel_radius[0]=30;
        pose.wheel_position[0][0]=-181;pose.wheel_position[0][1]=3153;pose.wheel_position[0][2]=-1320;
        chimney_try(1);
    }
    /* Repeat real adapter handoff/reacquisition with the same native pointers,
     * as a reused area can have on painting re-entry. No fresh() between visits.
     * Each visit lands on a different valid edge of the actual opening. */
    const float landings[][2]={{-275,-1486},{-86,-1486},{-181,-1580},{-181,-1391},{-181,-1486}};
    for(int role=NT_NONE;role<=NT_CLIENT;role++){
        chimney_setup(role);
        for(unsigned visit=0;visit<sizeof landings/sizeof *landings;visit++){
            chimney_position(landings[visit][0],3120.15f,landings[visit][1]);
            chimney_try(1);
            mario.action=ACT_DISAPPEARED;chimneyWarp.oInteractStatus=INT_STATUS_INTERACTED;
            CCM_CHECK(!rocket_adapter_update(&mario));
            CCM_CHECK(!player&&!haveFrame&&!meshHashes[0]&&!meshHashes[1]);
            object.numCollidedObjs=0;object.collidedObjInteractTypes=mario.collidedObjInteractTypes=0;
            testArea.index=2;mario.action=ACT_FREEFALL;step();
            rocket_adapter_suspend();testArea.index=1;chimneyWarp.oInteractStatus=0;
            mario.action=ACT_FREEFALL;step();
            mario.skipWarpInteractionsTimer=1;chimney_try(0);
            mario.skipWarpInteractionsTimer=0; // native dispatch owns countdown
        }
    }
    chimney_setup(NT_SERVER);gCLIOpts.offline=true;gNetworkAreaLoaded=false;gNetworkAreaSyncing=true;
    gNetworkPlayerLocal=NULL;chimney_try(1); // --offline still runs the engine as a server.
#define CCM_REJECT(change) do { chimney_setup(NT_CLIENT); change; chimney_try(0); } while(0)
    CCM_REJECT(gGlobalTimer+=2);
    CCM_REJECT(rocket_adapter_set_selected(0));
    CCM_REJECT(rocket_adapter_suspend());
    CCM_REJECT(enabled=0);
    CCM_REJECT(mario.playerIndex=1);
    CCM_REJECT(uiBlocked=1);
    CCM_REJECT(mario.health=0xff);
    CCM_REJECT(mario.hurtCounter=1);
    CCM_REJECT(mario.healCounter=1);
    CCM_REJECT(mario.squishTimer=1);
    CCM_REJECT(mario.quicksandDepth=1);
    CCM_REJECT(mario.input|=INPUT_SQUISHED);
    CCM_REJECT(mario.action=ACT_BACKWARD_GROUND_KB);
    CCM_REJECT(mario.action=ACT_READING_AUTOMATIC_DIALOG);
    CCM_REJECT(mario.action=ACT_DISAPPEARED);
    CCM_REJECT(mario.freeze=1);
    CCM_REJECT(mario.heldObj=&chimneyOther);
    CCM_REJECT(mario.heldByObj=&chimneyOther);
    CCM_REJECT(mario.riddenObj=&chimneyOther);
    CCM_REJECT(sCurrPlayMode=PLAY_MODE_PAUSED);
    CCM_REJECT(gWarpTransition.isActive=1);
    CCM_REJECT(sWarpDest.type=1);
    CCM_REJECT(sDelayedWarpOp=WARP_OP_WARP_OBJECT);
    CCM_REJECT(gTimeStopState=TIME_STOP_ACTIVE);
    CCM_REJECT(mario.skipWarpInteractionsTimer=1);
    CCM_REJECT(gCurrLevelNum=LEVEL_BOB);
    CCM_REJECT(testArea.index=2);
    CCM_REJECT(gCurrentArea=NULL);
    CCM_REJECT(gNetworkType=NT_NONE);
    CCM_REJECT(gNetworkAreaLoaded=false);
    CCM_REJECT(gNetworkAreaSyncing=true);
    CCM_REJECT(gNetworkPlayerLocal=NULL);
    CCM_REJECT(gNetworkPlayerLocal->connected=false);
    CCM_REJECT(gNetworkPlayerLocal->currLevelSyncValid=false);
    CCM_REJECT(gNetworkPlayerLocal->currAreaSyncValid=false);
    CCM_REJECT(gNetworkPlayerLocal->currLevelNum=LEVEL_BOB);
    CCM_REJECT(gNetworkPlayerLocal->currAreaIndex=2);
    CCM_REJECT(chimneyNode.node.id=0x0a);
    CCM_REJECT(chimneyNode.node.destLevel=LEVEL_BOB);
    CCM_REJECT(chimneyNode.node.destArea=1);
    CCM_REJECT(chimneyNode.node.destNode=0x0b);
    CCM_REJECT(chimneyNode.object=NULL);
    CCM_REJECT(chimneyWarp.behavior=bhvWingCap);
    CCM_REJECT(chimneyWarp.oBehParams^=0x10000);
    CCM_REJECT(chimneyWarp.oBehParams^=0x1000000);
    CCM_REJECT(chimneyWarp.activeFlags=0);
    CCM_REJECT(chimneyWarp.header.gfx.activeAreaIndex=2);
    CCM_REJECT(chimneyWarp.oInteractType=INTERACT_WARP_DOOR);
    CCM_REJECT(chimneyWarp.oIntangibleTimer=-1);
    CCM_REJECT(chimneyWarp.oInteractStatus=INT_STATUS_INTERACTED);
    CCM_REJECT(chimneyWarp.oPosX++);
    CCM_REJECT(chimneyWarp.oPosY++);
    CCM_REJECT(chimneyWarp.hitboxRadius=300);
    CCM_REJECT(chimneyWarp.hitboxHeight=100);
    CCM_REJECT(chimneyWarp.hitboxDownOffset=10);
    CCM_REJECT(chimney_position(-350,3120.15f,-1486));
    CCM_REJECT(chimney_position(-285,3120.15f,-1486));
    CCM_REJECT(chimney_position(-76,3120.15f,-1486));
    CCM_REJECT(chimney_position(-181,3120.15f,-1590));
    CCM_REJECT(chimney_position(-181,3120.15f,-1381));
    CCM_REJECT(chimney_position(-181,3120.15f,-1370));
    CCM_REJECT(chimney_position(-181,3100,-1486));
    CCM_REJECT(chimney_position(-181,3300,-1486));
    CCM_REJECT(chimney_position(-181,3140,-1486)); // In the nearby air with no physical rim contact.
    CCM_REJECT(mario.pos[1]+=3);
    CCM_REJECT(pose.position[0]=NAN);
    CCM_REJECT(pose.velocity[1]=NAN);
    CCM_REJECT(pose.velocity[1]=31);
    CCM_REJECT(pose.basis[7]=-.99f);
    CCM_REJECT(pose.flipping=1);
    CCM_REJECT(chimneyFloorHeight=3123);
    CCM_REJECT(nativeFloor=NULL);
    CCM_REJECT(chimneyBottom.type=SURFACE_DEFAULT);
    CCM_REJECT(chimneyBottom.vertex1[0]++);
    CCM_REJECT(chimneyBottom.object=&chimneyOther);
    CCM_REJECT(chimneyBottom.normal.y=NAN);
    CCM_REJECT(memset(gStaticSurfacePartition,0,sizeof gStaticSurfacePartition);pose.grounded=1);
    CCM_REJECT(for(int i=0;i<8;i++)chimneyRim[i].vertex1[0]++);
    CCM_REJECT(for(int i=0;i<8;i++)chimneyRim[i].type=SURFACE_DEFAULT);
    CCM_REJECT(for(int i=0;i<8;i++)chimneyRim[i].flags=SURFACE_FLAG_DYNAMIC);
    CCM_REJECT(for(int i=0;i<8;i++)chimneyRim[i].normal.y=NAN);
    CCM_REJECT(chimney_block(0,0));
    CCM_REJECT(chimney_block(1,0));
    CCM_REJECT(chimney_block(1,1);mario.flags=MARIO_VANISH_CAP;gLevelValues.fixVanishFloors=true);
    CCM_REJECT(object.numCollidedObjs=4;for(int i=0;i<4;i++)object.collidedObjs[i]=&chimneyOther);
    /* A wheel on the actual east rim, stored only across x=0, is accepted. */
    chimney_setup(NT_SERVER);chimney_position(-181,3157,-1486);pose.wheel_contacts[0]=1;pose.wheel_radius[0]=30;
    pose.wheel_position[0][0]=-20;pose.wheel_position[0][1]=3153;pose.wheel_position[0][2]=-1500;
    chimney_try(1);
    /* Normal Mario keeps all position, action and interaction state unchanged. */
    chimney_setup(NT_SERVER);rocket_adapter_set_selected(0);mario.action=ACT_IDLE;chimney_try(0);
    printf("PASS CCM chimney adapter: %u checks; exact loaded geometry, body/wheel rest, local host/client/offline, native-only staging and fail-closed controls\n",chimneyChecks);
    fresh();
#undef CCM_REJECT
}
#undef CCM_CHECK
