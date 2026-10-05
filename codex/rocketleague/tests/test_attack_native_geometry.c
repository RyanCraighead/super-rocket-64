/* Native transforms, triangle construction, floor query and enemy/switch loops.
 * Optional files are privately extracted owned geometry and RocketSim samples;
 * no game data is included. Audio, rendering, sockets and allocation are inert. */
#define ROCKET_ATTACK_NATIVE_GEOMETRY
#define ROCKET_SWITCH_FIXTURE_NO_MAIN
#include "test_switch_host.c"
#include <float.h>
#include "game/game_init.h"
#include "pc/utils/misc.h"
static unsigned checks,surfaceCount;
static struct Surface nativeSurfaces[64];
static struct SurfaceNode nativeNodes[64],floorHead;
SpatialPartitionCell gStaticSurfacePartition[NUM_CELLS][NUM_CELLS],gDynamicSurfacePartition[NUM_CELLS][NUM_CELLS];
s16 gCheckingSurfaceCollisionsForCamera;
u8 gInterpolatingSurfaces;
struct Object *gCheckingSurfaceCollisionsForObject;
f32 gRenderingDelta;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"native attack line %d: %s\n",__LINE__,#x);abort();}}while(0)
struct Surface *alloc_surface(s32 type){(void)type;CHECK(surfaceCount<64);struct Surface *s=&nativeSurfaces[surfaceCount++];memset(s,0,sizeof *s);return s;}
#include "attack_native_functions.inc.h"
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){
    float height=gLevelValues.floorLowerLimit;
    *floor=find_floor_from_list(floorHead.next,(s16)x,(s16)y,(s16)z,&height);return height;
}
void load_object_collision_model(void){
    surfaceCount=0;floorHead.next=NULL;memset(gDynamicSurfacePartition,0,sizeof gDynamicSurfacePartition);
    gCurrentObject=&enemy;enemy.header.gfx.throwMatrix=NULL;
    s16 *data=enemy.collisionData+1,vertices[3*64];CHECK(data[0]>0&&data[0]<=64);
    transform_object_vertices(&data,vertices);
    while(*data!=TERRAIN_LOAD_CONTINUE){
        s16 type=*data++,count=*data++;CHECK(count>0&&count<=32);
        for(int i=0;i<count;i++){
            struct Surface *s=read_surface_data(vertices,&data,SURFACE_POOL_DYNAMIC);data+=3;
            CHECK(s);s->object=&enemy;s->type=type;s->flags=SURFACE_FLAG_DYNAMIC;
            if(type==SURFACE_NO_CAM_COLLISION)s->flags|=SURFACE_FLAG_NO_CAM_COLLISION;
            unsigned n=surfaceCount-1;nativeNodes[n].surface=s;
            int part=s->normal.y>.01f?SPATIAL_PARTITION_FLOORS:s->normal.y<-.01f?SPATIAL_PARTITION_CEILS:SPATIAL_PARTITION_WALLS;
            nativeNodes[n].next=gDynamicSurfacePartition[8][8][part].next;gDynamicSurfacePartition[8][8][part].next=&nativeNodes[n];
        }
    }
    floorHead.next=gDynamicSurfacePartition[8][8][SPATIAL_PARTITION_FLOORS].next;
    collisionLoads++;
}
static s16 ownedWhomp[1024],ownedBlue[1024];
static RocketSnapshot blueSamples[120],kingPose;
static unsigned sampleCount;
static void read_collision(const char *path,s16 *buffer){FILE*f=fopen(path,"rb");CHECK(f);size_t n=fread(buffer,2,1024,f);CHECK(n>30&&n<1024&&feof(f));fclose(f);CHECK(buffer[0]==TERRAIN_LOAD_VERTICES&&buffer[1]==8);}
static void configure(int king,int role,int yaw){
    start(king,role!=NT_NONE);gNetworkType=role;
    if(role==NT_CLIENT){gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;}
    enemy.collisionData=ownedWhomp;enemy.oIntangibleTimer=-1;
    enemy.oFaceAngleYaw=yaw;enemy.oPosX=400;enemy.oPosY=200;enemy.oPosZ=-300;
    float scale=king?2.f:1.f;for(int k=0;k<3;k++)enemy.header.gfx.scale[k]=scale;
    memset(&gLevelValues,0,sizeof gLevelValues);gLevelValues.fixCollisionBugs=1;gLevelValues.floorLowerLimit=-11000;
    RocketWhompBack back;CHECK(whomp_back(&enemy,&back)&&back.eligible);
    localCar=kingPose;
    if(!king){ // native small and king use the same mesh, with different scale
        localCar.position[1]-=100;localCar.position[2]-=240;
        for(int i=0;i<4;i++){localCar.wheel_position[i][1]-=100;localCar.wheel_position[i][2]-=240;}
    }
    float c=coss(yaw),s=sins(yaw);
    for(int i=0;i<5;i++){
        float *p=i<4?localCar.wheel_position[i]:localCar.position,x=p[0],z=p[2];
        p[0]=400+c*x+s*z;p[1]+=200;p[2]=-300-s*x+c*z;
    }
    for(int i=0;i<3;i++){float x=localCar.basis[3*i],z=localCar.basis[3*i+2];localCar.basis[3*i]=c*x+s*z;localCar.basis[3*i+2]=-s*x+c*z;}
}
static void send_car(unsigned sequence){
    CharacterNetState s={0},d;uint8_t wire[CNET_WIRE_SIZE];s.kind=CNET_OCTANE;s.active=CNET_DRIVING;s.interaction=1;
    s.epoch=1;s.sequence=sequence;s.car=localCar;CHECK(character_net_encode(wire,sizeof wire,&s));CHECK(character_net_decode(&d,wire,sizeof wire));CHECK(character_net_accept(1,&d));
}
int main(int argc,char **argv){
    CHECK(argc==5);read_collision(argv[1],ownedWhomp);read_collision(argv[2],ownedBlue);
    FILE*f=fopen(argv[3],"rb");CHECK(f);sampleCount=fread(blueSamples,sizeof *blueSamples,120,f);CHECK(sampleCount>5&&sampleCount<120&&feof(f));fclose(f);
    f=fopen(argv[4],"rb");CHECK(f&&fread(&kingPose,sizeof kingPose,1,f)==1&&fgetc(f)==EOF);fclose(f);
    for(int king=0;king<2;king++)for(int role=NT_NONE;role<=NT_CLIENT;role++)for(int remote=0;remote<2;remote++)for(int yaw=0;yaw<65536;yaw+=8192){
        if(role==NT_NONE&&remote)continue;
        configure(king,role,yaw);localActive=!remote;
        for(unsigned stage=0;stage<(king?3u:1u);stage++){
            enemy.oAction=6;enemy.oSubAction=0;enemy.oTimer=10;enemy.oFaceAnglePitch=0x4000;
            localCar.ticks+=4;if(remote)send_car(stage+1);gGlobalTimer++;bhv_whomp_loop();
            if(king)CHECK(enemy.oHealth==2-(int)stage&&enemy.oSubAction==1);
            else CHECK(enemy.oAction==8&&loot==5);
            int hp=enemy.oHealth;CHECK(!rocket_whomp_ground_pound(&enemy)&&enemy.oHealth==hp);
        }
        if(king){CHECK(enemy.oAction==8);whomp_act_8();CHECK(stars==1&&enemy.oAction==9);}
    }
    for(int role=NT_NONE;role<=NT_CLIENT;role++)for(int variant=0;variant<5;variant++){
        switch_start(role!=NT_NONE);gNetworkType=role;enemy.collisionData=ownedBlue;
        memset(&gLevelValues,0,sizeof gLevelValues);gLevelValues.fixCollisionBugs=1;gLevelValues.floorLowerLimit=-11000;
        for(unsigned frame=0;frame<sampleCount;frame++){
            localCar=blueSamples[frame];
            if(variant==1)localCar.flipping=localCar.flipped=0;
            if(variant==2)localCar.position[0]+=1000;
            if(variant==3)authority=0; // switch events are local, independent of boss authority
            if(variant==4)localEpoch++; // never synthesize an entry across resets
            gGlobalTimer++;bhv_blue_coin_switch_loop();
        }
        CHECK(sends==((variant==0||variant==3)?1:0));
        CHECK(enemy.oAction==((variant==0||variant==3)?BLUE_COIN_SWITCH_ACT_RECEDING:BLUE_COIN_SWITCH_ACT_IDLE));
    }
    printf("PASS %u owned-geometry/actual-physics native integration assertions: Whomp King three hits/star, small loot, eight headings, offline/host/client/local/remote, blue flip activation and rejection\n",checks);
}
