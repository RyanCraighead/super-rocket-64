/* Real adapter boundary, generated rim by default. Optional owned geometry
 * and actual-backend snapshots are supplied privately as file arguments. */
#define main adapter_suite_main
#include "test_adapter.c"
#undef main
static unsigned pipeChecks;
#define PIPE_CHECK(x) do{pipeChecks++;if(!(x)){fprintf(stderr,"pipe line %d: %s\n",__LINE__,#x);abort();}}while(0)
static struct Object pipeObject,pipeOther;
static struct ObjectWarpNode pipeNode;
static struct Surface pipeSurfaces[1025];
static struct SurfaceNode pipeNodes[1025];
static RocketTriangle ownedTriangles[1024];
static unsigned ownedCount;
static RocketSnapshot ownedPoses[4];
static void pipe_pose(const RocketSnapshot *s){pose=*s;vec3f_copy(mario.pos,pose.position);vec3f_copy(lastPosition,mario.pos);}
static void pipe_geometry(void){
    memset(gStaticSurfacePartition,0,sizeof gStaticSurfacePartition);memset(gDynamicSurfacePartition,0,sizeof gDynamicSurfacePartition);
    memset(pipeSurfaces,0,sizeof pipeSurfaces);memset(pipeNodes,0,sizeof pipeNodes);gSurfaceNodesAllocated=2048;
    const RocketTriangle generated[]={{{{-150,205,100},{150,205,150},{150,205,100}},0},{{{-150,205,100},{-150,205,150},{150,205,150}},0}};
    unsigned count=ownedCount?ownedCount:2;
    for(unsigned i=0;i<count;i++){
        const RocketTriangle *t=ownedCount?&ownedTriangles[i]:&generated[i];struct Surface *s=&pipeSurfaces[i];
        s->object=&pipeObject;s->flags=SURFACE_FLAG_DYNAMIC;s->type=SURFACE_WALL_MISC;
        s16 *v[]={s->vertex1,s->vertex2,s->vertex3};for(int a=0;a<3;a++)for(int k=0;k<3;k++)v[a][k]=(s16)t->v[a][k];
        float a[3],b[3],normal[3];for(int k=0;k<3;k++){a[k]=t->v[1][k]-t->v[0][k];b[k]=t->v[2][k]-t->v[0][k];}
        float size=0;for(int k=0;k<3;k++){normal[k]=a[(k+1)%3]*b[(k+2)%3]-a[(k+2)%3]*b[(k+1)%3];size+=normal[k]*normal[k];}
        size=sqrtf(size);PIPE_CHECK(size>0);s->normal.x=normal[0]/size;s->normal.y=normal[1]/size;s->normal.z=normal[2]/size;
        int part=s->normal.y>.01f?SPATIAL_PARTITION_FLOORS:s->normal.y<-.01f?SPATIAL_PARTITION_CEILS:SPATIAL_PARTITION_WALLS;
        pipeNodes[i].surface=s;pipeNodes[i].next=gDynamicSurfacePartition[8][8][part].next;
        gDynamicSurfacePartition[8][8][part].next=&pipeNodes[i];
    }
}
static void pipe_setup(int which,int role,int heading){
    fresh();memset(&pipeObject,0,sizeof pipeObject);memset(&pipeOther,0,sizeof pipeOther);memset(&pipeNode,0,sizeof pipeNode);
    gCurrLevelNum=which==0?LEVEL_BITDW:which==1?LEVEL_BITS:LEVEL_THI;testArea.index=which<5?1:2;
    pipeNode.node.id=which<2?0x0b:0x32+(which-2)%3;
    pipeNode.node.destLevel=which==0?LEVEL_BOWSER_1:which==1?LEVEL_BOWSER_3:LEVEL_THI;
    pipeNode.node.destArea=which<2?1:3-testArea.index;pipeNode.node.destNode=which<2?0x0a:pipeNode.node.id;
    pipeObject.behavior=bhvWarpPipe;pipeObject.collisionData=(Collision*)warp_pipe_seg3_collision_03009AC8;
    pipeObject.activeFlags=ACTIVE_FLAG_ACTIVE;pipeObject.header.gfx.activeAreaIndex=testArea.index;
    vec3f_set(pipeObject.header.gfx.scale,1,1,1);pipeObject.oInteractType=INTERACT_WARP;
    pipeObject.oBehParams=(u32)pipeNode.node.id<<16;pipeObject.hitboxRadius=pipeObject.hitboxHeight=50;
    pipeNode.object=&pipeObject;testArea.warpNodes=&pipeNode;object.hitboxRadius=37;
    step();mario.action=ACT_FREEFALL;
    RocketSnapshot car={0};car.position[1]=219;float tilt=-.186f,up=sqrtf(1-tilt*tilt);
    car.basis[1]=tilt;car.basis[2]=up;car.basis[3]=1;car.basis[7]=up;car.basis[8]=-tilt;
    pipe_pose(ownedCount?&ownedPoses[heading]:&car);pipe_geometry();
    gCLIOpts.characterNet=role!=NT_NONE;gCLIOpts.offline=role==NT_NONE;gNetworkType=role;
    gNetworkAreaLoaded=true;gNetworkPlayerLocal=&gNetworkPlayers[0];
    gNetworkPlayerLocal->connected=gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=true;
    gNetworkPlayerLocal->currLevelNum=gCurrLevelNum;gNetworkPlayerLocal->currAreaIndex=testArea.index;
}
static void pipe_try(int expected){
    struct MarioState before=mario;struct Object actor=pipeObject;int count=object.numCollidedObjs,frames=steps,epoch=resets;
    rocket_adapter_prepare_interactions(&mario);
    PIPE_CHECK(steps==frames&&resets==epoch&&!memcmp(&actor,&pipeObject,sizeof actor));
    PIPE_CHECK(!memcmp(before.pos,mario.pos,sizeof mario.pos)&&mario.action==before.action&&mario.actionArg==before.actionArg);
    PIPE_CHECK(mario.usedObj==before.usedObj&&mario.interactObj==before.interactObj&&mario.numStars==before.numStars);
    if(expected)PIPE_CHECK(object.numCollidedObjs==count+1&&object.collidedObjs[count]==&pipeObject);
    else PIPE_CHECK(object.numCollidedObjs==count&&mario.collidedObjInteractTypes==before.collidedObjInteractTypes);
}
#define PIPE_REJECT(change) do{pipe_setup(0,NT_NONE,0);change;pipe_try(0);}while(0)
static void blockers(int dynamic){
    pipe_setup(0,NT_NONE,0);struct Surface*s=&pipeSurfaces[1024];s->type=SURFACE_DEFAULT;s->normal.y=1;
    vec3s_set(s->vertex1,-200,100,-200);vec3s_set(s->vertex2,200,100,-200);vec3s_set(s->vertex3,0,100,300);
    pipeNodes[1024].surface=s;
    SpatialPartitionCell(*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
    pipeNodes[1024].next=partition[8][8][SPATIAL_PARTITION_FLOORS].next;partition[8][8][SPATIAL_PARTITION_FLOORS].next=&pipeNodes[1024];pipe_try(0);
    mario.flags=MARIO_VANISH_CAP;s->type=SURFACE_VANISH_CAP_WALLS;pipe_try(0);
}
static void load_owned(const char *mesh,const char *prefix){
    FILE*f=fopen(mesh,"rb");PIPE_CHECK(f&&fread(&ownedCount,4,1,f)==1&&ownedCount<=1024&&ownedCount>0);
    for(unsigned i=0;i<ownedCount;i++){unsigned kind;PIPE_CHECK(fread(&kind,4,1,f)==1&&kind==SURFACE_WALL_MISC);PIPE_CHECK(fread(ownedTriangles[i].v,sizeof ownedTriangles[i].v,1,f)==1);}
    PIPE_CHECK(fgetc(f)==EOF);fclose(f);
    for(int i=0;i<4;i++){char path[2048];snprintf(path,sizeof path,"%s%d",prefix,i);f=fopen(path,"rb");PIPE_CHECK(f&&fread(&ownedPoses[i],sizeof(RocketSnapshot),1,f)==1&&fgetc(f)==EOF);fclose(f);}
}
int main(int argc,char **argv){
    if(argc==3)load_owned(argv[1],argv[2]);else PIPE_CHECK(argc==1);
    for(int route=0;route<8;route++)for(int role=NT_NONE;role<=NT_CLIENT;role++)for(int yaw=0;yaw<(ownedCount?4:1);yaw++){
        for(int visit=0;visit<3;visit++){
            pipe_setup(route,role,yaw);mario.numStars=visit?120:0;pipe_try(1);
            int count=object.numCollidedObjs;rocket_adapter_prepare_interactions(&mario);PIPE_CHECK(object.numCollidedObjs==count);
            mario.action=ACT_DISAPPEARED;PIPE_CHECK(!rocket_adapter_update(&mario));
        }
    }
    PIPE_REJECT(gCurrLevelNum=LEVEL_BITFS);PIPE_REJECT(pipeNode.node.destArea++);PIPE_REJECT(pipeNode.node.destNode++);
    PIPE_REJECT(pipeNode.object=NULL);PIPE_REJECT(pipeObject.behavior=bhvWarp);PIPE_REJECT(pipeObject.collisionData=NULL);
    PIPE_REJECT(pipeObject.activeFlags=0);PIPE_REJECT(pipeObject.oIntangibleTimer=-1);PIPE_REJECT(pipeObject.oSyncDeath=1);
    PIPE_REJECT(pipeObject.oInteractionSubtype=INT_SUBTYPE_FADING_WARP);PIPE_REJECT(pipeObject.header.gfx.scale[0]=2);
    PIPE_REJECT(pipeObject.oFaceAnglePitch=1);PIPE_REJECT(pipeObject.oInteractStatus=INT_STATUS_INTERACTED);
    PIPE_REJECT(pipeObject.hitboxRadius=200);PIPE_REJECT(pipeObject.oBehParams|=0x01000000);
    PIPE_REJECT(mario.skipWarpInteractionsTimer=1);PIPE_REJECT(mario.action=ACT_EMERGE_FROM_PIPE);
    PIPE_REJECT(mario.action=ACT_DISAPPEARED);PIPE_REJECT(mario.health=0xff);PIPE_REJECT(mario.playerIndex=1);
    PIPE_REJECT(mario.freeze=1);PIPE_REJECT(mario.hurtCounter=1);PIPE_REJECT(mario.healCounter=1);PIPE_REJECT(mario.squishTimer=1);
    PIPE_REJECT(mario.heldObj=&pipeObject);PIPE_REJECT(mario.heldByObj=&pipeObject);PIPE_REJECT(mario.riddenObj=&pipeObject);
    PIPE_REJECT(gWarpTransition.isActive=1);PIPE_REJECT(sDelayedWarpOp=WARP_OP_WARP_OBJECT);
    PIPE_REJECT(sWarpDest.type=WARP_TYPE_CHANGE_AREA);PIPE_REJECT(gTimeStopState=TIME_STOP_ACTIVE);
    PIPE_REJECT(sCurrPlayMode=PLAY_MODE_PAUSED);PIPE_REJECT(uiBlocked=1);PIPE_REJECT(pose.flipping=1);
    PIPE_REJECT(pose.velocity[1]=31);PIPE_REJECT(pose.position[1]+=500;vec3f_copy(mario.pos,pose.position));
    PIPE_REJECT(pose.position[0]=100;vec3f_copy(mario.pos,pose.position));PIPE_REJECT(mario.pos[0]+=3);
    PIPE_REJECT(object.numCollidedObjs=4);
    pipe_setup(0,NT_CLIENT,0);gNetworkAreaSyncing=true;pipe_try(0);
    pipe_setup(0,NT_CLIENT,0);gNetworkPlayerLocal->currLevelSyncValid=false;pipe_try(0);
    pipe_setup(0,NT_SERVER,0);gNetworkPlayerLocal->currAreaIndex++;pipe_try(0);
    blockers(0);blockers(1);
    printf("PASS %u standard-pipe adapter checks (%s): eight routes, native-only staging, offline/host/client, repeat visits and rejection gates\n",pipeChecks,ownedCount?"owned geometry + real physics poses":"generated fixture");
}
