/* Both directions: custom local controller -> RocketSim -> actual player packet
 * writer/ingress -> remote snapshot. Host services are inert; no socket/game QA. */
#include "character_transport_fixture.h"
#include "../../../src/pc/rocket_bindings.h"
#include <math.h>

static RocketInput local_input(int custom, int frame) {
    RocketBindings b=rocket_default_bindings;
    RocketPadSample raw={0};RocketGamepad pad={0};RocketInput keyboard={0};
    if(custom) {
        b.action[RA_BOOST]=RB_RB;b.action[RA_JUMP]=RB_NORTH;
        b.action[RA_SLIDE]=b.action[RA_ROLL]=RB_LB;b.stick=1;
    }
    if(frame>=30&&frame<105)raw.right_trigger=28000;
    if(frame>=35&&frame<90)raw.buttons|=1u<<(custom?10:1);
    if(frame>=100&&frame<108)raw.left_trigger=32767;
    if((frame>=115&&frame<117)||frame==120)raw.buttons|=1u<<(custom?3:0);
    if(frame>=120&&frame<130) {
        if(custom)raw.right_y=-25000;else raw.left_y=-25000;
    }
    if(frame>=90&&frame<100) {
        raw.buttons|=1u<<(custom?9:2);
        if(custom)raw.right_x=18000;else raw.left_x=18000;
    }
    pad.connected=pad.isolated=1;
    rocket_bindings_apply(&b,&raw,&pad);
    return rocket_gamepad_merge(&keyboard,&pad);
}
static void context(int source_is_client, int receiving) {
    int local=(source_is_client?1:0)^receiving;
    gNetworkType=local?NT_CLIENT:NT_SERVER;
    gNetworkPlayers[0].globalIndex=local;
    gNetworkPlayers[1].globalIndex=1-local;
    gNetworkPlayerServer=&gNetworkPlayers[local?1:0];
}
static void transfer(const RocketSnapshot *source,int source_is_client) {
    context(source_is_client,0);
    sourceSnapshot=source;
    for(int k=0;k<3;k++) {
        gMarioStates[0].pos[k]=source->position[k];gMarioStates[0].vel[k]=source->velocity[k]/30.f;
    }
    struct Packet p={0};capturedPacket=&p;network_send_player(0);capturedPacket=NULL;
    CHECK(!p.error&&!p.writeError&&p.dataLength>CNET_WIRE_SIZE);
    /* Changing the receiving PC's bindings must never remap a remote car. */
    configRocketBindings.action[RA_THROTTLE]=RB_NONE;
    configRocketBindings.action[RA_BOOST]=RB_NONE;
    context(source_is_client,1);p.localIndex=1;
    p.buffer[4]=source_is_client?PACKET_DESTINATION_BROADCAST:gNetworkPlayerLocal->globalIndex;p.cursor=3;
    int before=accepted;packet_receive(&p);CHECK(accepted==before+1);
    CHECK(character_net_remote_update(&gMarioStates[1]));
    const float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const int viewport[4]={0,0,1280,720};
    unsigned drawn=drawCalls;fixtureNow+=.21;
    character_net_draw(matrix,matrix,viewport);fixtureNow-=.21;
    CHECK(drawCalls==drawn+1&&drawnSnapshot.ticks==source->ticks);
    for(int k=0;k<3;k++)CHECK(fabsf(drawnSnapshot.position[k]-source->position[k])<.001f);
    CHECK(drawnSnapshot.boost==source->boost&&drawnSnapshot.jumped==source->jumped);
    CHECK(drawnSnapshot.flipped==source->flipped&&drawnSnapshot.flip_time==source->flip_time);
    rocket_bindings_reset();
}
int main(void) {
    static struct NetworkSystem system={.get_id_str=id,.requireServerBroadcast=true};
    gNetworkSystem=&system;gCLIOpts.characterNet=gCLIOpts.rocketCar=true;
    gNetworkAreaLoaded=true;gNetworkPlayerLocal=&gNetworkPlayers[0];
    for(int i=0;i<2;i++) {
        gNetworkPlayers[i].connected=true;gNetworkPlayers[i].localIndex=i;
        gNetworkPlayers[i].currAreaSyncValid=gNetworkPlayers[i].currLevelSyncValid=true;
        gMarioStates[i].playerIndex=i;gMarioStates[i].marioObj=&objects[i];gMarioStates[i].controller=&controllers[i];
        gMarioStates[i].action=ACT_IDLE;gMarioStates[i].health=0x880;
    }
    const RocketTriangle floor[]={
        {{{-20000,0,-20000},{20000,0,20000},{20000,0,-20000}}},
        {{{-20000,0,-20000},{-20000,0,20000},{20000,0,20000}}}
    };
    for(int role=0;role<2;role++) {
        character_net_clear_all();sourceEpoch++;
        RocketWorld *standard=rocket_world_create(),*custom=rocket_world_create();CHECK(standard&&custom);
        CHECK(rocket_world_mesh(standard,0,floor,2));CHECK(rocket_world_mesh(custom,0,floor,2));
        const float pos[]={0,40,0},vel[]={0,0,0};
        CHECK(rocket_world_reset(standard,pos,vel,0));CHECK(rocket_world_reset(custom,pos,vel,0));
        int saw_boost=0,saw_flip=0;
        for(int frame=0;frame<160;frame++) {
            RocketInput a=local_input(0,frame),b=local_input(1,frame);
            CHECK(!memcmp(&a,&b,sizeof(a)));
            CHECK(rocket_world_frame(standard,frame+1,&a,0,0)==4);
            CHECK(rocket_world_frame(custom,frame+1,&b,0,0)==4);
            RocketSnapshot sa,sb;CHECK(rocket_world_snapshot(standard,&sa));CHECK(rocket_world_snapshot(custom,&sb));
            for(int k=0;k<3;k++)CHECK(fabsf(sa.position[k]-sb.position[k])<.001f);
            for(int k=0;k<9;k++)CHECK(fabsf(sa.basis[k]-sb.basis[k])<.00001f);
            saw_boost|=sb.boost<95;saw_flip|=sb.flipped;
            fixtureNow+=1./30;transfer(&sb,role);
        }
        CHECK(saw_boost&&saw_flip);
        rocket_world_destroy(standard);rocket_world_destroy(custom);
    }
    printf("PASS bindings network: %d checks; custom host and client actions match defaults through RocketSim and native packet ingress (no sockets/windows)\n",checks);
}
