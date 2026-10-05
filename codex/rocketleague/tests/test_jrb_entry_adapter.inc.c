/* Production adapter with explicit native/runtime mocks. Synthetic tunnel
 * surfaces use the entrance dimensions; no game assets are bundled here. */
static struct Object shipObject,shipEel,shipSegment;
static struct ObjectWarpNode shipNode;
static struct Surface shipFaces[3];
static struct SurfaceNode shipFaceNodes[3];
static unsigned shipChecks;
#define SHIP_CHECK(c) do { ++shipChecks; assert(c); } while(0)
static void ship_test_world(const float local[3],float world[3]) {
    for(int k=0;k<3;k++) {
        world[k]=shipObject.transform[3][k];
        for(int a=0;a<3;a++)world[k]+=local[a]*shipObject.transform[a][k];
    }
}
static void ship_position(float x,float y,float z) {
    float local[3]={x,y,z},center[3];ship_test_world(local,center);
    for(int k=0;k<3;k++)pose.position[k]=mario.pos[k]=center[k]-
        pose.basis[k]*ROCKET_BODY_FORWARD_OFFSET-pose.basis[6+k]*ROCKET_BODY_UP_OFFSET;
}
static void ship_add_face(int i,int type,const float a[3],const float b[3],const float c[3],int dynamic) {
    struct Surface *s=&shipFaces[i];memset(s,0,sizeof *s);
    const float *local[]={a,b,c};s16 *v[]={s->vertex1,s->vertex2,s->vertex3};
    for(int j=0;j<3;j++) {float world[3];ship_test_world(local[j],world);for(int k=0;k<3;k++)v[j][k]=(s16)world[k];}
    s->object=dynamic?&shipObject:NULL;s->type=type;s->flags=dynamic?SURFACE_FLAG_DYNAMIC:0;s->normal.y=.8f;
    shipFaceNodes[i].surface=s;
    SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
    shipFaceNodes[i].next=partition[0][0][SPATIAL_PARTITION_FLOORS].next;
    partition[0][0][SPATIAL_PARTITION_FLOORS].next=&shipFaceNodes[i];
}
static void ship_setup(enum NetworkType role) {
    fresh();gCurrLevelNum=LEVEL_JRB;testArea.index=1;nativeWater=mario.waterLevel=1000;
    mario.pos[1]=-4700;step();mario.action=ACT_WATER_IDLE;pose.grounded=0;
    memset(&shipObject,0,sizeof shipObject);memset(&shipEel,0,sizeof shipEel);memset(&shipSegment,0,sizeof shipSegment);
    memset(&shipNode,0,sizeof shipNode);memset(shipFaceNodes,0,sizeof shipFaceNodes);
    memset(gStaticSurfacePartition,0,sizeof gStaticSurfacePartition);memset(gDynamicSurfacePartition,0,sizeof gDynamicSurfacePartition);
    shipObject.behavior=bhvInSunkenShip;shipObject.activeFlags=ACTIVE_FLAG_ACTIVE;shipObject.header.gfx.activeAreaIndex=1;
    shipObject.oPosX=5385;shipObject.oPosY=-5520;shipObject.oPosZ=2428;
    shipObject.oFaceAnglePitch=(s16)0xe958;shipObject.oFaceAngleYaw=(s16)0xee6c;shipObject.oFaceAngleRoll=0xc80;
    /* Native lookup-table angles truncate their low four bits. */
    float sx=sinf((0xe958>>4)*6.28318530718f/4096),cx=cosf((0xe958>>4)*6.28318530718f/4096);
    float sy=sinf((0xee6c>>4)*6.28318530718f/4096),cy=cosf((0xee6c>>4)*6.28318530718f/4096);
    float sz=sinf((0x0c80>>4)*6.28318530718f/4096),cz=cosf((0x0c80>>4)*6.28318530718f/4096);
    float rotation[3][3]={{cy*cz+sx*sy*sz,cx*sz,-sy*cz+sx*cy*sz},
        {-cy*sz+sx*sy*cz,cx*cz,sy*sz+sx*cy*cz},{cx*sy,-sx,cx*cy}};
    for(int a=0;a<3;a++) {shipObject.header.gfx.scale[a]=1;for(int k=0;k<3;k++)shipObject.transform[a][k]=rotation[a][k];}
    shipObject.transform[3][0]=5385;shipObject.transform[3][1]=-5520;shipObject.transform[3][2]=2428;
    /* Upright chassis approaches the tilted mouth from the water. */
    float length=hypotf(rotation[2][0],rotation[2][2]);memset(pose.basis,0,sizeof pose.basis);
    pose.basis[0]=-rotation[2][0]/length;pose.basis[2]=-rotation[2][2]/length;
    pose.basis[3]=pose.basis[2];pose.basis[5]=-pose.basis[0];pose.basis[7]=1;
    ship_position(409.5f,921.5f,-30);
    shipEel.behavior=bhvUnagi;shipEel.activeFlags=ACTIVE_FLAG_ACTIVE;shipEel.header.gfx.activeAreaIndex=1;shipEel.oAction=1;
    objectLists[OBJ_LIST_SURFACE].next=&shipObject.header;shipObject.header.next=&objectLists[OBJ_LIST_SURFACE];
    objectLists[OBJ_LIST_GENACTOR].next=&shipEel.header;shipEel.header.next=&objectLists[OBJ_LIST_GENACTOR];
    const float a[3]={512,819,-409},b[3]={307,819,-409},c[3]={307,819,-255},d[3]={512,819,-255};
    ship_add_face(0,SURFACE_WARP,a,b,c,1);ship_add_face(1,SURFACE_WARP,a,c,d,1);
    shipNode.node.id=WARP_NODE_WARP_FLOOR;shipNode.node.destLevel=LEVEL_JRB;shipNode.node.destArea=2;shipNode.node.destNode=0x0a;
    testArea.warpNodes=&shipNode;
    gNetworkType=role;gCLIOpts.characterNet=role!=NT_NONE;gCLIOpts.offline=role==NT_NONE;
    gNetworkAreaLoaded=true;gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerLocal->connected=true;
    gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=true;
    gNetworkPlayerLocal->currLevelNum=LEVEL_JRB;gNetworkPlayerLocal->currAreaIndex=1;
}
static void ship_try(int accepted) {
    struct MarioState before=mario;struct Object oldShip=shipObject,oldEel=shipEel;
    int beforeSteps=steps,beforeResets=resets,beforeWarps=shipWarps;
    rocket_adapter_prepare_interactions(&mario);
    SHIP_CHECK(shipWarps==beforeWarps+accepted);
    SHIP_CHECK(!memcmp(&before,&mario,sizeof mario));
    SHIP_CHECK(!memcmp(&oldShip,&shipObject,sizeof shipObject)&&!memcmp(&oldEel,&shipEel,sizeof shipEel));
    SHIP_CHECK(steps==beforeSteps&&resets==beforeResets&&!object.numCollidedObjs);
}
static void ship_blocker(int dynamic,int type) {
    const float a[3]={250,750,-180},b[3]={570,750,-180},c[3]={410,1150,-180};
    ship_add_face(2,type,a,b,c,dynamic);
}
static void test_jrb_entry_adapter(void) {
    /* Reproduce the physical obstruction: sweep an upright chassis toward the
     * tilted tunnel until its box first contacts the right wall. Its center is
     * still outside the native warp floor, but the entry bridge must work. */
    ship_setup(NT_NONE);
    const float side[3][3]={{512,819,-101},{512,1024,-101},{512,819,-409}};
    float triangle[3][3];for(int v=0;v<3;v++)ship_test_world(side[v],triangle[v]);
    float z=80;
    for(;z>-101;z-=.25f){ship_position(409.5f,921.5f,z);if(rocket_car_triangle_overlap(&pose,triangle,0))break;}
    SHIP_CHECK(z>-101); // Native warp begins at local z=-255.
    ship_add_face(2,SURFACE_DEFAULT,side[0],side[1],side[2],1);
    ship_try(1);
    for(int role=NT_NONE;role<=NT_CLIENT;role++) {
        ship_setup(role);ship_try(1);ship_try(0); // Native pending warp prevents duplicate requests.
    }
    ship_setup(NT_SERVER);gCLIOpts.offline=true;gNetworkPlayerLocal=NULL;gNetworkAreaLoaded=false;ship_try(1);
#define SHIP_REJECT(change) do { ship_setup(NT_CLIENT);change;ship_try(0); } while(0)
    SHIP_REJECT(gCurrActNum=2);SHIP_REJECT(gCurrLevelNum=LEVEL_CCM);SHIP_REJECT(testArea.index=2);
    SHIP_REJECT(gCurrentArea=NULL);SHIP_REJECT(mario.playerIndex=1);SHIP_REJECT(mario.health=0xff);
    SHIP_REJECT(mario.action=ACT_READING_AUTOMATIC_DIALOG);SHIP_REJECT(mario.action=ACT_BACKWARD_GROUND_KB);
    SHIP_REJECT(mario.freeze=1);SHIP_REJECT(mario.heldObj=&shipEel);SHIP_REJECT(mario.heldByObj=&shipEel);
    SHIP_REJECT(mario.riddenObj=&shipEel);SHIP_REJECT(uiBlocked=1);SHIP_REJECT(enabled=0);
    SHIP_REJECT(rocket_adapter_set_selected(0));SHIP_REJECT(rocket_adapter_suspend());SHIP_REJECT(gGlobalTimer+=2);
    SHIP_REJECT(mario.hurtCounter=1);SHIP_REJECT(mario.healCounter=1);SHIP_REJECT(mario.squishTimer=1);
    SHIP_REJECT(mario.quicksandDepth=1);SHIP_REJECT(mario.input|=INPUT_SQUISHED);SHIP_REJECT(mario.skipWarpInteractionsTimer=1);
    SHIP_REJECT(sCurrPlayMode=PLAY_MODE_PAUSED);SHIP_REJECT(gWarpTransition.isActive=1);
    SHIP_REJECT(sWarpDest.type=1);SHIP_REJECT(sDelayedWarpOp=WARP_OP_WARP_OBJECT);SHIP_REJECT(gTimeStopState=TIME_STOP_ACTIVE);
    SHIP_REJECT(gNetworkType=NT_NONE);SHIP_REJECT(gNetworkAreaLoaded=false);SHIP_REJECT(gNetworkAreaSyncing=true);
    SHIP_REJECT(gNetworkPlayerLocal=NULL);SHIP_REJECT(gNetworkPlayerLocal->connected=false);
    SHIP_REJECT(gNetworkPlayerLocal->currLevelSyncValid=false);SHIP_REJECT(gNetworkPlayerLocal->currAreaSyncValid=false);
    SHIP_REJECT(gNetworkPlayerLocal->currLevelNum=LEVEL_CCM);SHIP_REJECT(gNetworkPlayerLocal->currAreaIndex=2);
    SHIP_REJECT(shipNode.node.id=0xf0);SHIP_REJECT(shipNode.node.destLevel=LEVEL_CCM);
    SHIP_REJECT(shipNode.node.destArea=1);SHIP_REJECT(shipNode.node.destNode=0xb);
    SHIP_REJECT(shipEel.oAction=0);SHIP_REJECT(shipEel.oAction=3);SHIP_REJECT(shipEel.activeFlags=0);
    SHIP_REJECT(shipEel.oBehParams2ndByte=1);SHIP_REJECT(shipEel.header.gfx.activeAreaIndex=2);
    SHIP_REJECT(shipObject.activeFlags=0);SHIP_REJECT(shipObject.behavior=bhvWarp);SHIP_REJECT(shipObject.oPosY++);
    SHIP_REJECT(shipObject.oFaceAnglePitch++);SHIP_REJECT(shipObject.header.gfx.scale[0]=2);
    SHIP_REJECT(shipObject.transform[0][0]=NAN);SHIP_REJECT(shipObject.transform[0][0]*=2);
    SHIP_REJECT(shipObject.transform[3][0]++);
    SHIP_REJECT(ship_position(600,921.5f,-30));SHIP_REJECT(ship_position(200,921.5f,-30));
    SHIP_REJECT(ship_position(409.5f,1100,-30));SHIP_REJECT(ship_position(409.5f,700,-30));
    SHIP_REJECT(ship_position(409.5f,921.5f,200));SHIP_REJECT(ship_position(409.5f,921.5f,-350));
    SHIP_REJECT(mario.pos[0]+=3);SHIP_REJECT(pose.position[0]=NAN);SHIP_REJECT(pose.basis[0]=NAN);
    SHIP_REJECT(pose.flipping=1);SHIP_REJECT(mario.waterLevel=-6000);
    SHIP_REJECT(shipFaces[0].type=SURFACE_DEFAULT);SHIP_REJECT(shipFaces[1].object=NULL);
    SHIP_REJECT(shipFaces[0].flags=SURFACE_FLAG_INTANGIBLE);SHIP_REJECT(shipFaces[0].normal.y=NAN);
    SHIP_REJECT(shipFaces[0].vertex1[0]+=10);SHIP_REJECT(gSurfaceNodesAllocated=1);
    SHIP_REJECT(shipFaceNodes[0].next=&shipFaceNodes[0]);
    SHIP_REJECT(ship_blocker(0,SURFACE_DEFAULT));SHIP_REJECT(ship_blocker(1,SURFACE_DEFAULT));
    SHIP_REJECT(ship_blocker(1,SURFACE_VANISH_CAP_WALLS);mario.flags=MARIO_VANISH_CAP);
    ship_setup(NT_CLIENT);shipSegment.behavior=bhvUnagiSubobject;shipSegment.activeFlags=ACTIVE_FLAG_ACTIVE;
    shipSegment.header.gfx.activeAreaIndex=1;shipSegment.oPosX=pose.position[0];shipSegment.oPosY=pose.position[1];shipSegment.oPosZ=pose.position[2];
    shipEel.header.next=&shipSegment.header;shipSegment.header.next=&objectLists[OBJ_LIST_GENACTOR];ship_try(0);
    shipSegment.oPosX+=1000;ship_try(1);
    printf("PASS JRB entrance adapter: %u checks; tilted mouth, loaded warp, eel progression, blockers, local offline/host/client and native-only warp\n",shipChecks);
    fresh();
#undef SHIP_REJECT
}
#undef SHIP_CHECK
