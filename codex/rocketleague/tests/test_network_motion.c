/* RocketSim trajectory -> native player writer -> native ingress/apply ->
 * production interpolation/render snapshot. No SDL, sockets or OS input.
 * Host adaptation is a fixture; this does NOT establish in-game acceptance. */
#include "character_transport_fixture.h"
#include <math.h>

static RocketWorld *world;
static RocketSnapshot source;
static RocketInput input;
static uint64_t frame;
static unsigned sentCount, deliveredCount, reorderedCount, lostCount;
static struct Packet delayed;
static int hasDelayed;
static double lastDelivery;
static uint32_t lastEpoch;
static int lastActive;

static void client_context(void) {
    gNetworkType=NT_CLIENT;
    gNetworkPlayers[0].globalIndex=1;
    gNetworkPlayers[1].globalIndex=0;
    gNetworkPlayerServer=&gNetworkPlayers[1];
}
static struct Packet write_source(int active) {
    struct Packet result={0};
    gNetworkType=NT_SERVER;
    gNetworkPlayers[0].globalIndex=0;
    gNetworkPlayers[1].globalIndex=1;
    gNetworkPlayerServer=&gNetworkPlayers[0];
    sourceSnapshot=active==CNET_DRIVING?&source:NULL;
    presentationSnapshot=active==CNET_PRESENTATION?&source:NULL;
    gMarioStates[0].action=source.water_mode==ROCKET_WATER_JET?ACT_WATER_IDLE:
        source.water_mode==ROCKET_WATER_METAL?ACT_METAL_WATER_FALLING:ACT_IDLE;
    gMarioStates[0].flags=source.water_mode==ROCKET_WATER_METAL?MARIO_METAL_CAP:0;
    /* This fixture mirrors host position/velocity only. It does not test the
     * Mario adapter, door interaction, focus gates or native socket transport. */
    for(int k=0;k<3;k++){
        gMarioStates[0].pos[k]=source.position[k];
        gMarioStates[0].vel[k]=source.velocity[k]/30.f;
    }
    capturedPacket=&result;
    network_send_player(0);
    capturedPacket=NULL;
    CHECK(result.dataLength>CNET_WIRE_SIZE&&!result.error&&!result.writeError);
    CharacterNetState decoded;
    CHECK(character_net_decode(&decoded,result.buffer+result.dataLength-CNET_WIRE_SIZE,CNET_WIRE_SIZE));
    CHECK(decoded.active==active&&decoded.epoch==sourceEpoch);
    if(active){
        CharacterNetState expected=decoded;expected.car=source;
        uint8_t bytes[CNET_WIRE_SIZE];
        CHECK(character_net_encode(bytes,sizeof bytes,&expected));
        CHECK(!memcmp(bytes,result.buffer+result.dataLength-CNET_WIRE_SIZE,sizeof bytes));
    }
    client_context();
    /* The separate host cap-grant fixture owns lease validation. This motion
     * fixture supplies its already-verified receiver lease explicitly. */
    verifiedCapFlags[1]=gMarioStates[0].flags&MARIO_SPECIAL_CAPS;
    /* Physical sender and concrete destination supplied by native transport. */
    result.localIndex=1;result.buffer[4]=1;result.cursor=3;
    sentCount++;
    return result;
}
static int draw_at(double when) {
    double saved=fixtureNow;fixtureNow=when;
    unsigned before=drawCalls;
    const float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const int viewport[4]={0,0,1280,720};
    character_net_draw(matrix,matrix,viewport);
    fixtureNow=saved;
    return drawCalls!=before;
}
static void same_snapshot(const RocketSnapshot *a,const RocketSnapshot *b) {
    for(int k=0;k<3;k++){
        CHECK(fabsf(a->position[k]-b->position[k])<.01f);
        CHECK(fabsf(a->velocity[k]-b->velocity[k])<.01f);
        CHECK(fabsf(a->angular_velocity[k]-b->angular_velocity[k])<.0001f);
    }
    for(int k=0;k<9;k++)CHECK(fabsf(a->basis[k]-b->basis[k])<.0001f);
    for(int w=0;w<4;w++){
        for(int k=0;k<3;k++)CHECK(fabsf(a->wheel_position[w][k]-b->wheel_position[w][k])<.02f);
        CHECK(a->wheel_contacts[w]==b->wheel_contacts[w]);
        CHECK(fabsf(a->wheel_radius[w]-b->wheel_radius[w])<.0001f);
        CHECK(fabsf(a->wheel_steer[w]-b->wheel_steer[w])<.0001f);
    }
    CHECK(a->ticks==b->ticks&&a->boost==b->boost&&a->jump_time==b->jump_time);
    CHECK(a->flip_time==b->flip_time&&a->air_time==b->air_time);
    CHECK(a->grounded==b->grounded&&a->jumped==b->jumped);
    CHECK(a->water_mode==b->water_mode);
    CHECK(a->double_jumped==b->double_jumped&&a->flipped==b->flipped&&a->flipping==b->flipping);
}
static void receive(struct Packet packet,int active) {
    int previousDraw=draw_at(fixtureNow);
    RocketSnapshot previous=drawnSnapshot;
    struct MarioState localBefore=gMarioStates[0];
    RocketSnapshot sourceBefore=source;
    int before=accepted;
    packet_receive(&packet);
    CHECK(accepted==before+1&&!packet.error);
    deliveredCount++;
    CHECK(!memcmp(&localBefore,&gMarioStates[0],sizeof localBefore));
    if(active){
        CHECK(objects[1].oIntangibleTimer==-1);
        CHECK(character_net_remote_update(&gMarioStates[1]));
        CHECK(draw_at(fixtureNow));
        if(previousDraw&&lastActive==active&&lastEpoch==sourceEpoch){
            /* Continuous position, then a quarter of the clamped arrival
             * interval. Rotation must remain proper during actual flips. */
            for(int k=0;k<3;k++)CHECK(fabsf(drawnSnapshot.position[k]-previous.position[k])<.01f);
            double duration=fmin(fmax(fixtureNow-lastDelivery,1./30),.2);
            CHECK(draw_at(fixtureNow+duration*.25));
            for(int k=0;k<3;k++)CHECK(fabsf(drawnSnapshot.position[k]-(previous.position[k]+(source.position[k]-previous.position[k])*.25f))<.02f);
            CharacterNetState sampled={0};sampled.speed_percent=100;sampled.kind=CNET_OCTANE;sampled.active=active;sampled.car=drawnSnapshot;
            uint8_t bytes[CNET_WIRE_SIZE];CHECK(character_net_encode(bytes,sizeof bytes,&sampled));
        }else same_snapshot(&drawnSnapshot,&source);
        CHECK(draw_at(fixtureNow+.21));
        same_snapshot(&drawnSnapshot,&source);
    }else{
        CHECK(!character_net_remote_update(&gMarioStates[1]));
        CHECK(objects[1].oIntangibleTimer==0&&!draw_at(fixtureNow));
    }
    CHECK(rocket_world_snapshot(world,&source));same_snapshot(&source,&sourceBefore);
    CHECK(!memcmp(&localBefore,&gMarioStates[0],sizeof localBefore));
    lastDelivery=fixtureNow;lastEpoch=sourceEpoch;lastActive=active;
}
static void stale(struct Packet packet) {
    packet.cursor=3;
    int before=accepted;
    struct MarioState remoteBefore=gMarioStates[1];
    packet_receive(&packet);
    CHECK(accepted==before&&!memcmp(&remoteBefore,&gMarioStates[1],sizeof remoteBefore));
    reorderedCount++;
}
static void transmit(int active,int impair) {
    fixtureNow+=1./30+(sentCount%2?.004:-.004);
    struct Packet packet=write_source(active);
    if(impair&&sentCount%7==0){lostCount++;return;}
    if(impair&&sentCount%11==0&&!hasDelayed){delayed=packet;hasDelayed=1;return;}
    receive(packet,active);
    if(impair&&sentCount%5==0)stale(packet);
    if(hasDelayed){stale(delayed);hasDelayed=0;}
}
static void step(unsigned count,int impair) {
    while(count--){
        CHECK(rocket_world_frame(world,++frame,&input,0,0)==4);
        CHECK(rocket_world_snapshot(world,&source));transmit(1,impair);
    }
}
static void reset(float height) {
    const float pos[]={0,height,0},vel[]={0,0,0};
    CHECK(rocket_world_reset(world,pos,vel,0));
    sourceEpoch++;frame=0;memset(&input,0,sizeof input);
    CHECK(rocket_world_snapshot(world,&source));transmit(1,0);
}
int main(void) {
    static struct NetworkSystem system={.get_id_str=id,.requireServerBroadcast=true};
    gNetworkSystem=&system;gCLIOpts.characterNet=gCLIOpts.rocketCar=true;
    gNetworkAreaLoaded=true;gNetworkPlayerLocal=&gNetworkPlayers[0];
    for(int i=0;i<2;i++){
        gNetworkPlayers[i].connected=true;gNetworkPlayers[i].localIndex=i;
        gNetworkPlayers[i].currAreaSyncValid=gNetworkPlayers[i].currLevelSyncValid=true;
        gMarioStates[i].playerIndex=i;gMarioStates[i].marioObj=&objects[i];
        gMarioStates[i].controller=&controllers[i];gMarioStates[i].action=ACT_IDLE;
        gMarioStates[i].health=0x880;
    }
    const RocketTriangle floor[]={
        {{{-20000,0,-20000},{20000,0,20000},{20000,0,-20000}}},
        {{{-20000,0,-20000},{-20000,0,20000},{20000,0,20000}}}
    };
    world=rocket_world_create();CHECK(world);CHECK(rocket_world_mesh(world,0,floor,2));
    reset(40);step(30,1);CHECK(source.grounded);
    input.throttle=input.boost=1;step(80,1);
    CHECK(source.velocity[2]>4300&&source.boost<15);
    input.boost=0;input.steer=.8f;input.powerslide=1;step(20,1);
    CHECK(fabsf(source.position[0])>100);
    reset(40);step(30,1);input.jump=1;step(3,1);input.jump=0;step(1,0);
    input.jump=1;input.pitch=-1;step(1,0);CHECK(source.flipped&&source.flipping);
    input.jump=0;input.pitch=1;step(7,1);CHECK(source.flip_time>0);
    input.pitch=0;step(100,1);CHECK(source.grounded);
    reset(5000);input.pitch=.7f;input.yaw=.6f;input.roll=.8f;step(18,1);
    CHECK(fabsf(source.basis[7]-1)>.1f);
    RocketSnapshot paused=source;input.jump=input.boost=1;
    for(int i=0;i<12;i++){
        CHECK(!rocket_world_frame(world,++frame,&input,1,0));
        CHECK(rocket_world_snapshot(world,&source));same_snapshot(&source,&paused);transmit(1,1);
    }
    step(1,0);CHECK(!source.flipped&&!source.double_jumped&&source.boost==paused.boost);
    memset(&input,0,sizeof input);step(1,0);input.jump=1;input.pitch=-1;step(1,0);CHECK(source.flipped);
    /* Native ownership release/reacquisition, not an actual door cutscene. */
    transmit(CNET_PRESENTATION,0);transmit(CNET_PRESENTATION,0);
    transmit(CNET_DRIVING,0); /* activity changes snap even inside one epoch */
    float retainedBoost=source.boost;
    transmit(0,0);reset(40);CHECK(source.ticks==0&&!source.flipped&&source.boost==retainedBoost);
    step(30,1);CHECK(source.grounded);
    CHECK(!draw_at(fixtureNow+1.01));
    fixtureNow+=1.1;step(1,0);CHECK(draw_at(fixtureNow));
    retainedBoost=source.boost;
    reset(1000);rocket_world_set_water(world,1,10000,0);input.boost=1;step(80,1);
    CHECK(source.water_mode==ROCKET_WATER_JET&&source.boost==retainedBoost);
    CHECK(character_net_water_mode(1)==ROCKET_WATER_JET);
    input.pitch=.5f;input.yaw=.4f;step(30,1);CHECK(fabsf(source.basis[7]-1)>.1f);
    rocket_world_set_water(world,1,10000,1);step(10,0);
    CHECK(source.water_mode==ROCKET_WATER_METAL&&character_net_water_mode(1)==ROCKET_WATER_METAL);
    rocket_world_set_water(world,0,0,0);step(5,0);
    CHECK(source.water_mode==ROCKET_WATER_DRY&&character_net_water_mode(1)==ROCKET_WATER_DRY);
    rocket_world_set_water(world,1,10000,0);step(5,0);
    character_net_clear(1);CHECK(!character_net_water_mode(1));step(1,0);
    CHECK(character_net_water_mode(1)==ROCKET_WATER_JET);
    /* Environment trajectories use the same pose channel; observers never apply
     * wind/current a second time. Exercise loss/replay while entering/leaving. */
    reset(5000);RocketEnvironment environment={{180,0,60},{0,4500,0}};
    CHECK(rocket_world_set_environment(world,&environment));step(20,1);
    CHECK(source.position[0]>100&&source.position[2]>30);
    CHECK(rocket_world_set_environment(world,NULL));step(10,1);
    RocketTriangle slick[2];memcpy(slick,floor,sizeof slick);
    slick[0].material=slick[1].material=ROCKET_MATERIAL_VERY_SLIPPERY;
    CHECK(rocket_world_mesh(world,0,slick,2));rocket_world_set_surface_mode(world,ROCKET_SURFACES_NATIVE);
    reset(40);step(30,1);input.throttle=1;step(30,1);CHECK(source.position[2]>50);
    rocket_world_set_surface_mode(world,ROCKET_SURFACES_CAR);step(20,1);
    CHECK(rocket_world_mesh(world,0,floor,2));step(10,1);
    transmit(0,0);reset(40);step(30,1);
    CHECK(lostCount>20&&reorderedCount>20&&deliveredCount>200);
    rocket_world_destroy(world);
    printf("network motion: %d checks; %u sent, %u applied, %u dropped, %u duplicate/older rejected\n",checks,sentCount,deliveredCount,lostCount,reorderedCount);
    puts("PASS: real RocketSim/writer/ingress/interpolation; NOT native driving, socket delivery or internet acceptance");
    return 0;
}
